/**
 * @file LooseOctree.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/Frustum.h"
#include "math/box.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::data::geometry {

/**
 * @brief
 *  Loose octree of boxes keyed by a small integer id (an entity index), for culling and area queries.
 *
 * A node of half size `h` accepts the boxes whose centre lies in its cell and whose half extent is at most `h`: its
 * loose bounds (twice the cell) always enclose them, so a box that moves is re-filed without splitting anything. The
 * root grows to take a box outside it; nodes left empty by removals are released. A flat box (a 2D sprite) is a box of
 * zero depth: the same tree serves the 2D and 3D layers.
 */
class OWL_API LooseOctree final {
public:
	/**
	 * @brief
	 *  Starting shape of the tree.
	 */
	struct Config {
		/// Centre of the root cell.
		math::vec3 centre{0.f, 0.f, 0.f};
		/// Half size of the root cell; the root doubles when a box falls outside.
		float halfSize = 64.f;
		/// Deepest level below the root.
		uint32_t maxDepth = 10;
		/// Smallest half size of a cell: small boxes share a cell instead of a chain of cells of one box each.
		float minHalfSize = 8.f;
	};

	/**
	 * @brief
	 *  Counters of the last query, for diagnostics and benchmarks.
	 */
	struct QueryStatistics {
		/// Nodes whose loose bounds were tested.
		uint32_t nodesTested = 0;
		/// Boxes tested one by one (those of the nodes crossing the query volume).
		uint32_t boxesTested = 0;
		/// Ids returned.
		uint32_t hits = 0;
	};

	/**
	 * @brief
	 *  Copy constructor.
	 */
	LooseOctree(const LooseOctree&) = default;
	/**
	 * @brief
	 *  Move constructor.
	 */
	LooseOctree(LooseOctree&&) = default;
	/**
	 * @brief
	 *  Copy assignment.
	 * @return This tree.
	 */
	auto operator=(const LooseOctree&) -> LooseOctree& = default;
	/**
	 * @brief
	 *  Move assignment.
	 * @return This tree.
	 */
	auto operator=(LooseOctree&&) -> LooseOctree& = default;
	/**
	 * @brief
	 *  Build an empty tree of the default shape.
	 */
	LooseOctree();
	/**
	 * @brief
	 *  Build an empty tree.
	 * @param[in] iConfig Starting shape.
	 */
	explicit LooseOctree(const Config& iConfig);
	/**
	 * @brief
	 *  Destructor.
	 */
	~LooseOctree() = default;

	/**
	 * @brief
	 *  Insert a box, or move the box already filed under this id.
	 * @param[in] iId Id of the box.
	 * @param[in] iBox The box.
	 */
	void insert(uint32_t iId, const math::box3f& iBox);

	/**
	 * @brief
	 *  Remove the box of an id; nothing happens when the id is absent.
	 * @param[in] iId Id of the box.
	 */
	void remove(uint32_t iId);

	/**
	 * @brief
	 *  Remove every box and go back to the starting shape.
	 */
	void clear();

	/**
	 * @brief
	 *  Check whether an id has a box.
	 * @param[in] iId The id.
	 * @return True when the id is filed.
	 */
	[[nodiscard]] auto contains(uint32_t iId) const -> bool;

	/**
	 * @brief
	 *  Get the box of an id.
	 * @param[in] iId The id, which must be filed (`contains`).
	 * @return The box.
	 */
	[[nodiscard]] auto getBox(uint32_t iId) const -> const math::box3f&;

	/**
	 * @brief
	 *  Get the number of boxes.
	 * @return The count.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_count; }

	/**
	 * @brief
	 *  Get the number of live nodes, root included.
	 * @return The count.
	 */
	[[nodiscard]] auto getNodeCount() const -> size_t { return m_nodes.size() - m_freeNodes.size(); }

	/**
	 * @brief
	 *  Get the half size of the root cell (it grows with the content).
	 * @return The half size.
	 */
	[[nodiscard]] auto getRootHalfSize() const -> float;

	/**
	 * @brief
	 *  Append the ids whose box touches a frustum.
	 * @param[in] iFrustum The frustum.
	 * @param[in,out] ioIds The ids found are appended, in no particular order.
	 */
	void query(const math::Frustum& iFrustum, std::vector<uint32_t>& ioIds) const;

	/**
	 * @brief
	 *  Append the ids whose box touches a box.
	 * @param[in] iBox The query box.
	 * @param[in,out] ioIds The ids found are appended, in no particular order.
	 */
	void query(const math::box3f& iBox, std::vector<uint32_t>& ioIds) const;

	/**
	 * @brief
	 *  Get the counters of the last query.
	 * @return The counters.
	 */
	[[nodiscard]] auto getLastQueryStatistics() const -> const QueryStatistics& { return m_lastQuery; }

private:
	/// Index of no node.
	static constexpr uint32_t g_noNode = 0xFFFFFFFFu;

	/**
	 * @brief
	 *  A cell of the tree.
	 */
	struct Node {
		/// Centre of the cell.
		math::vec3 centre{0.f, 0.f, 0.f};
		/// Half size of the cell; the loose bounds are twice as large.
		float halfSize = 0.f;
		/// Parent node, `g_noNode` for the root.
		uint32_t parent = g_noNode;
		/// Depth below the root.
		uint32_t depth = 0;
		/// Children by octant (bit 0: +x, bit 1: +y, bit 2: +z), `g_noNode` when absent.
		std::array<uint32_t, 8> children{g_noNode, g_noNode, g_noNode, g_noNode,
										 g_noNode, g_noNode, g_noNode, g_noNode};
		/// Boxes filed in this node, stored here so a query reads them in sequence.
		std::vector<math::box3f> boxes;
		/// Id of each box of `boxes`.
		std::vector<uint32_t> items;
		/// Boxes in this node and below.
		uint32_t subtreeCount = 0;
	};

	/**
	 * @brief
	 *  Where an id is filed.
	 */
	struct Entry {
		/// Node holding the id, `g_noNode` when absent.
		uint32_t node = g_noNode;
		/// Position in the boxes and items of the node.
		uint32_t slot = 0;
	};

	/**
	 * @brief
	 *  Get the node a box belongs to, creating the missing cells on the way.
	 * @param[in] iBox The box.
	 * @return The node index.
	 */
	auto findOrCreateNode(const math::box3f& iBox) -> uint32_t;

	/**
	 * @brief
	 *  Double the root toward a point until its cell holds it.
	 * @param[in] iPoint The point.
	 */
	void growToward(const math::vec3& iPoint);

	/**
	 * @brief
	 *  Get a node slot, reusing a released one.
	 * @param[in] iCentre Centre of the cell.
	 * @param[in] iHalfSize Half size of the cell.
	 * @param[in] iParent Parent node.
	 * @param[in] iDepth Depth of the node.
	 * @return The node index.
	 */
	auto allocateNode(const math::vec3& iCentre, float iHalfSize, uint32_t iParent, uint32_t iDepth) -> uint32_t;

	/**
	 * @brief
	 *  Take an id out of its node and release the nodes left empty.
	 * @param[in,out] ioEntry The entry of the id.
	 */
	void detach(Entry& ioEntry);

	/**
	 * @brief
	 *  Add `iDelta` to the subtree count of a node and its ancestors.
	 * @param[in] iNode The node.
	 * @param[in] iDelta Change of the count.
	 */
	void addToSubtree(uint32_t iNode, int32_t iDelta);

	/**
	 * @brief
	 *  Append every id of a subtree.
	 * @param[in] iNode Root of the subtree.
	 * @param[in,out] ioIds Ids appended.
	 */
	void collect(uint32_t iNode, std::vector<uint32_t>& ioIds) const;

	/**
	 * @brief
	 *  Get the loose bounds of a node.
	 * @param[in] iNode The node.
	 * @return The bounds.
	 */
	[[nodiscard]] auto looseBounds(const Node& iNode) const -> math::box3f;

	/// Starting shape, restored by `clear`.
	Config m_config;
	/// Node pool; index 0 is not special, `m_root` is.
	std::vector<Node> m_nodes;
	/// Released node slots.
	std::vector<uint32_t> m_freeNodes;
	/// Entries indexed by id.
	std::vector<Entry> m_entries;
	/// Root node.
	uint32_t m_root = 0;
	/// Boxes filed.
	size_t m_count = 0;
	/// Depth limit below the current root (grows with the root).
	uint32_t m_maxDepth = 0;
	/// Counters of the last query.
	mutable QueryStatistics m_lastQuery;
	/// Scratch stack of the queries.
	mutable std::vector<uint32_t> m_stack;
};

}// namespace owl::data::geometry

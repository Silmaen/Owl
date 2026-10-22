/**
 * @file LooseOctree.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/geometry/LooseOctree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace owl::data::geometry {

namespace {
constexpr uint32_t g_maxGrowth = 32;

auto boxCentre(const math::box3f& iBox) -> math::vec3 { return (iBox.min() + iBox.max()) * 0.5f; }

auto boxHalfExtent(const math::box3f& iBox) -> float {
	const math::vec3 extent = iBox.max() - iBox.min();
	return std::max({extent.x(), extent.y(), extent.z()}) * 0.5f;
}

auto octantOf(const math::vec3& iCentre, const math::vec3& iPoint) -> uint32_t {
	return (iPoint.x() >= iCentre.x() ? 1u : 0u) | (iPoint.y() >= iCentre.y() ? 2u : 0u) |
		   (iPoint.z() >= iCentre.z() ? 4u : 0u);
}

auto octantCentre(const math::vec3& iCentre, const float iChildHalf, const uint32_t iOctant) -> math::vec3 {
	return math::vec3{iCentre.x() + ((iOctant & 1u) != 0 ? iChildHalf : -iChildHalf),
					  iCentre.y() + ((iOctant & 2u) != 0 ? iChildHalf : -iChildHalf),
					  iCentre.z() + ((iOctant & 4u) != 0 ? iChildHalf : -iChildHalf)};
}

auto cellHolds(const math::vec3& iCentre, const float iHalf, const math::vec3& iPoint) -> bool {
	return std::abs(iPoint.x() - iCentre.x()) <= iHalf && std::abs(iPoint.y() - iCentre.y()) <= iHalf &&
		   std::abs(iPoint.z() - iCentre.z()) <= iHalf;
}
}// namespace

LooseOctree::LooseOctree() : LooseOctree{Config{}} {}

LooseOctree::LooseOctree(const Config& iConfig) : m_config{iConfig} { clear(); }

void LooseOctree::clear() {
	++m_generation;
	m_nodes.clear();
	m_freeNodes.clear();
	m_entries.clear();
	m_count = 0;
	m_maxDepth = m_config.maxDepth;
	m_root = allocateNode(m_config.centre, std::max(m_config.halfSize, 1e-3f), g_noNode, 0);
}

auto LooseOctree::allocateNode(const math::vec3& iCentre, const float iHalfSize, const uint32_t iParent,
							   const uint32_t iDepth) -> uint32_t {
	uint32_t index = 0;
	if (!m_freeNodes.empty()) {
		index = m_freeNodes.back();
		m_freeNodes.pop_back();
	} else {
		index = static_cast<uint32_t>(m_nodes.size());
		m_nodes.emplace_back();
	}
	auto& node = m_nodes[index];
	node.centre = iCentre;
	node.halfSize = iHalfSize;
	node.parent = iParent;
	node.depth = iDepth;
	node.children.fill(g_noNode);
	node.boxes.clear();
	node.items.clear();
	node.subtreeCount = 0;
	return index;
}

void LooseOctree::growToward(const math::vec3& iPoint) {
	for (uint32_t step = 0; step < g_maxGrowth; ++step) {
		const math::vec3 oldCentre = m_nodes[m_root].centre;
		const float oldHalf = m_nodes[m_root].halfSize;
		const uint32_t oldCount = m_nodes[m_root].subtreeCount;
		if (cellHolds(oldCentre, oldHalf, iPoint))
			return;
		// The old root becomes the octant of the new root that lies away from the point.
		const math::vec3 newCentre{oldCentre.x() + (iPoint.x() >= oldCentre.x() ? oldHalf : -oldHalf),
								   oldCentre.y() + (iPoint.y() >= oldCentre.y() ? oldHalf : -oldHalf),
								   oldCentre.z() + (iPoint.z() >= oldCentre.z() ? oldHalf : -oldHalf)};
		const uint32_t oldRoot = m_root;
		m_root = allocateNode(newCentre, oldHalf * 2.f, g_noNode, 0);
		auto& root = m_nodes[m_root];
		root.children[octantOf(newCentre, oldCentre)] = oldRoot;
		root.subtreeCount = oldCount;
		m_nodes[oldRoot].parent = m_root;
		++m_maxDepth;
		// Depths are relative to the root: every node below goes one level down.
		m_stack.clear();
		m_stack.push_back(oldRoot);
		while (!m_stack.empty()) {
			const uint32_t index = m_stack.back();
			m_stack.pop_back();
			auto& node = m_nodes[index];
			++node.depth;
			for (const uint32_t child: node.children) {
				if (child != g_noNode)
					m_stack.push_back(child);
			}
		}
	}
	OWL_CORE_WARN("LooseOctree: Box too far from the root ({}, {}, {}), filed at the root.", iPoint.x(), iPoint.y(),
				  iPoint.z())
}

auto LooseOctree::findOrCreateNode(const math::box3f& iBox) -> uint32_t {
	const math::vec3 centre = boxCentre(iBox);
	growToward(centre);
	const float halfExtent = boxHalfExtent(iBox);
	uint32_t index = m_root;
	while (m_nodes[index].depth < m_maxDepth) {
		const float childHalf = m_nodes[index].halfSize * 0.5f;
		if (halfExtent > childHalf || childHalf < m_config.minHalfSize)
			break;
		const uint32_t octant = octantOf(m_nodes[index].centre, centre);
		uint32_t child = m_nodes[index].children[octant];
		if (child == g_noNode) {
			const math::vec3 childCentre = octantCentre(m_nodes[index].centre, childHalf, octant);
			const uint32_t depth = m_nodes[index].depth + 1;
			child = allocateNode(childCentre, childHalf, index, depth);
			m_nodes[index].children[octant] = child;
		}
		index = child;
	}
	return index;
}

void LooseOctree::addToSubtree(uint32_t iNode, const int32_t iDelta) {
	while (iNode != g_noNode) {
		auto& node = m_nodes[iNode];
		node.subtreeCount = static_cast<uint32_t>(static_cast<int32_t>(node.subtreeCount) + iDelta);
		iNode = node.parent;
	}
}

void LooseOctree::insert(const uint32_t iId, const math::box3f& iBox) {
	++m_generation;
	if (iId >= m_entries.size())
		m_entries.resize(std::max<size_t>(iId + 1, m_entries.size() * 2));
	if (const auto& entry = m_entries[iId]; entry.node != g_noNode) {
		auto& node = m_nodes[entry.node];
		const float halfExtent = boxHalfExtent(iBox);
		const bool fitsHere = halfExtent <= node.halfSize && cellHolds(node.centre, node.halfSize, boxCentre(iBox));
		const float childHalf = node.halfSize * 0.5f;
		const bool fitsDeeper = node.depth < m_maxDepth && halfExtent <= childHalf && childHalf >= m_config.minHalfSize;
		if (fitsHere && !fitsDeeper) {
			node.boxes[entry.slot] = iBox;
			m_contentBounds.min() = math::vec3{std::min(m_contentBounds.min().x(), iBox.min().x()),
											   std::min(m_contentBounds.min().y(), iBox.min().y()),
											   std::min(m_contentBounds.min().z(), iBox.min().z())};
			m_contentBounds.max() = math::vec3{std::max(m_contentBounds.max().x(), iBox.max().x()),
											   std::max(m_contentBounds.max().y(), iBox.max().y()),
											   std::max(m_contentBounds.max().z(), iBox.max().z())};
			return;
		}
		detach(m_entries[iId]);
	}
	const uint32_t index = findOrCreateNode(iBox);
	auto& node = m_nodes[index];
	auto& filed = m_entries[iId];
	filed.node = index;
	filed.slot = static_cast<uint32_t>(node.items.size());
	node.boxes.push_back(iBox);
	node.items.push_back(iId);
	addToSubtree(index, 1);
	if (m_count == 0) {
		m_contentBounds = iBox;
	} else {
		m_contentBounds.min() = math::vec3{std::min(m_contentBounds.min().x(), iBox.min().x()),
										   std::min(m_contentBounds.min().y(), iBox.min().y()),
										   std::min(m_contentBounds.min().z(), iBox.min().z())};
		m_contentBounds.max() = math::vec3{std::max(m_contentBounds.max().x(), iBox.max().x()),
										   std::max(m_contentBounds.max().y(), iBox.max().y()),
										   std::max(m_contentBounds.max().z(), iBox.max().z())};
	}
	++m_count;
}

void LooseOctree::detach(Entry& ioEntry) {
	++m_generation;
	auto& node = m_nodes[ioEntry.node];
	const uint32_t last = node.items.back();
	node.items[ioEntry.slot] = last;
	node.boxes[ioEntry.slot] = node.boxes.back();
	m_entries[last].slot = ioEntry.slot;
	node.items.pop_back();
	node.boxes.pop_back();
	uint32_t index = ioEntry.node;
	addToSubtree(index, -1);
	--m_count;
	ioEntry.node = g_noNode;
	// Release the cells left empty, the root excepted.
	while (index != m_root && m_nodes[index].subtreeCount == 0) {
		const uint32_t parent = m_nodes[index].parent;
		auto& children = m_nodes[parent].children;
		std::ranges::replace(children, index, g_noNode);
		m_nodes[index].items.clear();
		m_nodes[index].boxes.clear();
		m_freeNodes.push_back(index);
		index = parent;
	}
}

void LooseOctree::remove(const uint32_t iId) {
	if (iId >= m_entries.size() || m_entries[iId].node == g_noNode)
		return;
	detach(m_entries[iId]);
}

auto LooseOctree::contains(const uint32_t iId) const -> bool {
	return iId < m_entries.size() && m_entries[iId].node != g_noNode;
}

auto LooseOctree::getBox(const uint32_t iId) const -> const math::box3f& {
	const auto& entry = m_entries[iId];
	return m_nodes[entry.node].boxes[entry.slot];
}

auto LooseOctree::getRootHalfSize() const -> float { return m_nodes[m_root].halfSize; }

auto LooseOctree::looseBounds(const Node& iNode) const -> math::box3f {
	const float loose = iNode.halfSize * 2.f;
	const math::vec3 extent{loose, loose, loose};
	return math::box3f{iNode.centre - extent, iNode.centre + extent};
}

void LooseOctree::collect(const uint32_t iNode, std::vector<uint32_t>& ioIds) const {
	const size_t base = m_stack.size();
	m_stack.push_back(iNode);
	while (m_stack.size() > base) {
		const uint32_t index = m_stack.back();
		m_stack.pop_back();
		const auto& node = m_nodes[index];
		ioIds.insert(ioIds.end(), node.items.begin(), node.items.end());
		m_lastQuery.hits += static_cast<uint32_t>(node.items.size());
		for (const uint32_t child: node.children) {
			if (child != g_noNode)
				m_stack.push_back(child);
		}
	}
}

void LooseOctree::query(const math::Frustum& iFrustum, std::vector<uint32_t>& ioIds) const {
	OWL_PROFILE_FUNCTION()

	m_lastQuery = {};
	m_stack.clear();
	m_stack.push_back(m_root);
	while (!m_stack.empty()) {
		const uint32_t index = m_stack.back();
		m_stack.pop_back();
		const auto& node = m_nodes[index];
		if (node.subtreeCount == 0)
			continue;
		++m_lastQuery.nodesTested;
		const auto containment = iFrustum.classify(looseBounds(node));
		if (containment == math::Containment::Outside)
			continue;
		if (containment == math::Containment::Inside) {
			collect(index, ioIds);
			continue;
		}
		m_lastQuery.boxesTested += static_cast<uint32_t>(node.boxes.size());
		for (size_t slot = 0; slot < node.boxes.size(); ++slot) {
			if (iFrustum.intersects(node.boxes[slot])) {
				ioIds.push_back(node.items[slot]);
				++m_lastQuery.hits;
			}
		}
		for (const uint32_t child: node.children) {
			if (child != g_noNode)
				m_stack.push_back(child);
		}
	}
}

void LooseOctree::query(const math::box3f& iBox, std::vector<uint32_t>& ioIds) const {
	OWL_PROFILE_FUNCTION()

	m_lastQuery = {};
	m_stack.clear();
	m_stack.push_back(m_root);
	while (!m_stack.empty()) {
		const uint32_t index = m_stack.back();
		m_stack.pop_back();
		const auto& node = m_nodes[index];
		if (node.subtreeCount == 0)
			continue;
		++m_lastQuery.nodesTested;
		if (!looseBounds(node).intersect(iBox))
			continue;
		m_lastQuery.boxesTested += static_cast<uint32_t>(node.boxes.size());
		for (size_t slot = 0; slot < node.boxes.size(); ++slot) {
			if (node.boxes[slot].intersect(iBox)) {
				ioIds.push_back(node.items[slot]);
				++m_lastQuery.hits;
			}
		}
		for (const uint32_t child: node.children) {
			if (child != g_noNode)
				m_stack.push_back(child);
		}
	}
}

}// namespace owl::data::geometry

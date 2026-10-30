/**
 * @file PcgEvaluate.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/pcg/PcgGraph.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace owl::scene::pcg {

namespace {

constexpr float g_maskThreshold = 0.5f;

// SplitMix64: the hash of every random stream, the same on every target.
constexpr auto mix(uint64_t iValue) -> uint64_t {
	iValue += 0x9e3779b97f4a7c15ull;
	iValue = (iValue ^ (iValue >> 30u)) * 0xbf58476d1ce4e5b9ull;
	iValue = (iValue ^ (iValue >> 27u)) * 0x94d049bb133111ebull;
	return iValue ^ (iValue >> 31u);
}

constexpr auto toUnit(const uint64_t iHash) -> float {
	return static_cast<float>(iHash >> 40u) / static_cast<float>(1u << 24u);
}

auto latticeValue(const uint64_t iSeed, const int64_t iX, const int64_t iY) -> float {
	return toUnit(mix(iSeed ^ mix(static_cast<uint64_t>(iX) ^ mix(static_cast<uint64_t>(iY)))));
}

auto valueNoise(const uint64_t iSeed, const float iX, const float iY) -> float {
	const float floorX = std::floor(iX);
	const float floorY = std::floor(iY);
	const auto cellX = static_cast<int64_t>(floorX);
	const auto cellY = static_cast<int64_t>(floorY);
	const float fracX = iX - floorX;
	const float fracY = iY - floorY;
	const float smoothX = fracX * fracX * (3.f - 2.f * fracX);
	const float smoothY = fracY * fracY * (3.f - 2.f * fracY);
	const float top = std::lerp(latticeValue(iSeed, cellX, cellY), latticeValue(iSeed, cellX + 1, cellY), smoothX);
	const float bottom =
			std::lerp(latticeValue(iSeed, cellX, cellY + 1), latticeValue(iSeed, cellX + 1, cellY + 1), smoothX);
	return std::lerp(top, bottom, smoothY);
}

auto toFloat(const std::string& iText, const float iFallback) -> float {
	float value = iFallback;
	if (std::from_chars(iText.data(), iText.data() + iText.size(), value).ec != std::errc{})
		return iFallback;
	return value;
}

auto toInt(const std::string& iText, const int64_t iFallback) -> int64_t {
	int64_t value = iFallback;
	if (const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), value);
		error != std::errc{}) {
		const float asFloat = toFloat(iText, static_cast<float>(iFallback));
		return static_cast<int64_t>(asFloat);
	}
	return value;
}

auto isSolid(const Field& iField, const int64_t iX, const int64_t iY, const bool iOutside) -> bool {
	if (iX < 0 || iY < 0 || iX >= static_cast<int64_t>(iField.size.x()) || iY >= static_cast<int64_t>(iField.size.y()))
		return iOutside;
	return iField.get(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY)) >= g_maskThreshold;
}

class Evaluator {
public:
	Evaluator(const Graph& iGraph, const Region& iRegion, Result& ioResult)
		: m_graph{iGraph}, m_region{iRegion}, m_result{ioResult} {}

	auto run() -> bool {
		const auto order = sortNodes();
		if (!order)
			return false;
		for (const auto* node: *order) m_result.values[node->id] = compute(*node);
		std::vector<const GraphNode*> outputs;
		for (const auto& node: m_graph.nodes) {
			if (node.type.starts_with("output."))
				outputs.push_back(&node);
		}
		std::ranges::sort(outputs, [](const GraphNode* iLhs, const GraphNode* iRhs) -> bool {
			return std::tuple{iLhs->position[1], iLhs->position[0], static_cast<uint64_t>(iLhs->id)} <
				   std::tuple{iRhs->position[1], iRhs->position[0], static_cast<uint64_t>(iRhs->id)};
		});
		for (const auto* node: outputs) writeOutput(*node);
		return true;
	}

private:
	[[nodiscard]] auto emptyField() const -> Field {
		return {.size = m_region.size, .values = std::vector<float>(m_region.size.surface(), 0.f)};
	}

	[[nodiscard]] auto nodeSeed(const GraphNode& iNode) const -> uint64_t {
		const auto offset = static_cast<uint64_t>(toInt(Graph::getValue(iNode, "seed"), 0));
		return mix(m_region.seed ^ mix(static_cast<uint64_t>(iNode.id) ^ mix(offset)));
	}

	[[nodiscard]] auto number(const GraphNode& iNode, const std::string_view iParam) const -> float {
		return toFloat(Graph::getValue(iNode, iParam), 0.f);
	}

	[[nodiscard]] auto integer(const GraphNode& iNode, const std::string_view iParam) const -> int64_t {
		return toInt(Graph::getValue(iNode, iParam), 0);
	}

	auto input(const GraphNode& iNode, const std::string& iPin) -> std::optional<PinValue> {
		const auto link = std::ranges::find_if(m_graph.links, [&](const GraphLink& iLink) -> bool {
			return iLink.toNode == iNode.id && iLink.toPin == iPin;
		});
		if (link != m_graph.links.end()) {
			if (const auto values = m_result.values.find(link->fromNode); values != m_result.values.end()) {
				if (const auto found = values->second.find(link->fromPin); found != values->second.end())
					return found->second;
			}
		}
		m_result.warnings.push_back(std::format("Input '{}' of node {} ({}) is not linked.", iPin,
												static_cast<uint64_t>(iNode.id), iNode.type));
		return std::nullopt;
	}

	auto fieldInput(const GraphNode& iNode, const std::string& iPin) -> Field {
		if (auto value = input(iNode, iPin); value && std::holds_alternative<Field>(*value))
			return std::get<Field>(std::move(*value));
		return emptyField();
	}

	auto pointsInput(const GraphNode& iNode, const std::string& iPin) -> Points {
		if (auto value = input(iNode, iPin); value && std::holds_alternative<Points>(*value))
			return std::get<Points>(std::move(*value));
		return {};
	}

	// Kahn's order: every node after the nodes its inputs read, none when a node depends on itself.
	[[nodiscard]] auto sortNodes() const -> std::optional<std::vector<const GraphNode*>> {
		std::map<core::UUID, size_t> pending;
		for (const auto& node: m_graph.nodes) pending[node.id] = 0;
		for (const auto& link: m_graph.links) {
			if (pending.contains(link.fromNode) && pending.contains(link.toNode))
				++pending[link.toNode];
		}
		std::vector<const GraphNode*> order;
		order.reserve(m_graph.nodes.size());
		for (const auto& node: m_graph.nodes) {
			if (pending[node.id] == 0)
				order.push_back(&node);
		}
		for (size_t next = 0; next < order.size(); ++next) {
			for (const auto& link: m_graph.links) {
				if (link.fromNode != order[next]->id || !pending.contains(link.toNode))
					continue;
				if (--pending[link.toNode] == 0)
					order.push_back(m_graph.findNode(link.toNode));
			}
		}
		if (order.size() != m_graph.nodes.size()) {
			OWL_CORE_WARN("PcgGraph: {} nodes depend on themselves.", m_graph.nodes.size() - order.size())
			return std::nullopt;
		}
		return order;
	}

	auto compute(const GraphNode& iNode) -> std::map<std::string, PinValue> {
		const auto& type = iNode.type;
		if (type == "source.noise")
			return {{"Field", noise(iNode)}};
		if (type == "source.random") {
			auto result = emptyField();
			const auto seed = nodeSeed(iNode);
			for (uint32_t y = 0; y < m_region.size.y(); ++y) {
				for (uint32_t x = 0; x < m_region.size.x(); ++x)
					result.values[static_cast<size_t>(y) * m_region.size.x() + x] = latticeValue(seed, x, y);
			}
			return {{"Field", result}};
		}
		if (type == "source.constant") {
			auto result = emptyField();
			std::ranges::fill(result.values, number(iNode, "value"));
			return {{"Field", result}};
		}
		if (type == "source.gradient")
			return {{"Field", gradient(iNode)}};
		if (type == "field.threshold") {
			auto result = fieldInput(iNode, "Field");
			const float low = number(iNode, "min");
			const float high = number(iNode, "max");
			for (auto& value: result.values) value = value >= low && value <= high ? 1.f : 0.f;
			return {{"Mask", result}};
		}
		if (type == "field.combine")
			return {{"Field", combine(iNode)}};
		if (type == "field.invert") {
			auto result = fieldInput(iNode, "Field");
			for (auto& value: result.values) value = 1.f - value;
			return {{"Field", result}};
		}
		if (type == "field.blur")
			return {{"Field", blur(fieldInput(iNode, "Field"), integer(iNode, "radius"))}};
		if (type == "field.cellular")
			return {{"Mask", cellular(iNode)}};
		if (type == "field.edge")
			return {{"Mask", edge(iNode)}};
		if (type == "points.scatter")
			return {{"Points", scatter(iNode)}};
		if (type == "points.filter") {
			auto result = pointsInput(iNode, "Points");
			const auto filter = fieldInput(iNode, "Field");
			const float low = number(iNode, "min");
			const float high = number(iNode, "max");
			std::erase_if(result.cells, [&](const math::vec2ui& iCell) -> bool {
				const float value = filter.get(iCell.x(), iCell.y());
				return value < low || value > high;
			});
			return {{"Points", result}};
		}
		if (type == "points.to_mask")
			return {{"Mask", pointsToMask(pointsInput(iNode, "Points"), integer(iNode, "radius"))}};
		if (type == "output.tiles" || type == "output.erase")
			return {{"Mask", fieldInput(iNode, "Mask")}};
		if (type == "output.points")
			return {{"Points", pointsInput(iNode, "Points")}};
		if (type == "output.height")
			return {{"Height", fieldInput(iNode, "Height")}};
		m_result.warnings.push_back(
				std::format("Node {} has the unknown type '{}'.", static_cast<uint64_t>(iNode.id), type));
		return {};
	}

	[[nodiscard]] auto noise(const GraphNode& iNode) const -> Field {
		auto result = emptyField();
		const float scale = std::max(1.f, number(iNode, "scale"));
		const auto octaves = std::clamp<int64_t>(integer(iNode, "octaves"), 1, 8);
		const float persistence = std::clamp(number(iNode, "persistence"), 0.f, 1.f);
		const auto seed = nodeSeed(iNode);
		for (uint32_t y = 0; y < m_region.size.y(); ++y) {
			for (uint32_t x = 0; x < m_region.size.x(); ++x) {
				float sum = 0.f;
				float weight = 0.f;
				float amplitude = 1.f;
				float frequency = 1.f / scale;
				for (int64_t octave = 0; octave < octaves; ++octave) {
					sum += amplitude * valueNoise(mix(seed + static_cast<uint64_t>(octave)),
												  static_cast<float>(x) * frequency, static_cast<float>(y) * frequency);
					weight += amplitude;
					amplitude *= persistence;
					frequency *= 2.f;
				}
				result.values[static_cast<size_t>(y) * m_region.size.x() + x] = weight > 0.f ? sum / weight : 0.f;
			}
		}
		return result;
	}

	[[nodiscard]] auto gradient(const GraphNode& iNode) const -> Field {
		auto result = emptyField();
		const auto direction = Graph::getValue(iNode, "direction");
		const float width = static_cast<float>(std::max(1u, m_region.size.x() - 1));
		const float height = static_cast<float>(std::max(1u, m_region.size.y() - 1));
		for (uint32_t y = 0; y < m_region.size.y(); ++y) {
			for (uint32_t x = 0; x < m_region.size.x(); ++x) {
				const float u = static_cast<float>(x) / width;
				const float v = static_cast<float>(y) / height;
				float value = u;
				if (direction == "vertical")
					value = v;
				else if (direction == "radial")
					value = std::min(1.f, std::hypot(u - 0.5f, v - 0.5f) * 2.f);
				result.values[static_cast<size_t>(y) * m_region.size.x() + x] = value;
			}
		}
		return result;
	}

	auto combine(const GraphNode& iNode) -> Field {
		auto result = fieldInput(iNode, "A");
		const auto other = fieldInput(iNode, "B");
		const auto operation = Graph::getValue(iNode, "operation");
		for (size_t i = 0; i < result.values.size() && i < other.values.size(); ++i) {
			auto& value = result.values[i];
			const float rhs = other.values[i];
			if (operation == "add")
				value += rhs;
			else if (operation == "subtract")
				value -= rhs;
			else if (operation == "multiply")
				value *= rhs;
			else if (operation == "min")
				value = std::min(value, rhs);
			else if (operation == "max")
				value = std::max(value, rhs);
			else
				value = (value + rhs) * 0.5f;
		}
		return result;
	}

	[[nodiscard]] auto blur(const Field& iField, const int64_t iRadius) const -> Field {
		auto result = emptyField();
		const auto radius = std::clamp<int64_t>(iRadius, 1, 16);
		const auto width = static_cast<int64_t>(m_region.size.x());
		const auto height = static_cast<int64_t>(m_region.size.y());
		for (int64_t y = 0; y < height; ++y) {
			for (int64_t x = 0; x < width; ++x) {
				float sum = 0.f;
				uint32_t count = 0;
				for (int64_t dy = -radius; dy <= radius; ++dy) {
					for (int64_t dx = -radius; dx <= radius; ++dx) {
						if (x + dx < 0 || y + dy < 0 || x + dx >= width || y + dy >= height)
							continue;
						sum += iField.get(static_cast<uint32_t>(x + dx), static_cast<uint32_t>(y + dy));
						++count;
					}
				}
				result.values[static_cast<size_t>(y * width + x)] = count > 0 ? sum / static_cast<float>(count) : 0.f;
			}
		}
		return result;
	}

	auto cellular(const GraphNode& iNode) -> Field {
		auto current = fieldInput(iNode, "Mask");
		const auto iterations = std::clamp<int64_t>(integer(iNode, "iterations"), 0, 16);
		const auto birth = integer(iNode, "birth");
		const auto survival = integer(iNode, "survival");
		const bool solidEdges = Graph::getValue(iNode, "edges") != "open";
		for (auto& value: current.values) value = value >= g_maskThreshold ? 1.f : 0.f;
		for (int64_t step = 0; step < iterations; ++step) {
			auto next = current;
			for (uint32_t y = 0; y < m_region.size.y(); ++y) {
				for (uint32_t x = 0; x < m_region.size.x(); ++x) {
					int64_t count = 0;
					for (int64_t dy = -1; dy <= 1; ++dy) {
						for (int64_t dx = -1; dx <= 1; ++dx) {
							if ((dx != 0 || dy != 0) && isSolid(current, x + dx, y + dy, solidEdges))
								++count;
						}
					}
					const bool alive = isSolid(current, x, y, solidEdges) ? count >= survival : count >= birth;
					next.values[static_cast<size_t>(y) * m_region.size.x() + x] = alive ? 1.f : 0.f;
				}
			}
			current = std::move(next);
		}
		return current;
	}

	auto edge(const GraphNode& iNode) -> Field {
		const auto mask = fieldInput(iNode, "Mask");
		const bool diagonals = Graph::getValue(iNode, "neighbours") != "4";
		auto result = emptyField();
		for (uint32_t y = 0; y < m_region.size.y(); ++y) {
			for (uint32_t x = 0; x < m_region.size.x(); ++x) {
				if (!isSolid(mask, x, y, false))
					continue;
				bool border = false;
				for (int64_t dy = -1; dy <= 1 && !border; ++dy) {
					for (int64_t dx = -1; dx <= 1 && !border; ++dx) {
						if ((dx == 0 && dy == 0) || (!diagonals && dx != 0 && dy != 0))
							continue;
						border = !isSolid(mask, x + dx, y + dy, false);
					}
				}
				result.values[static_cast<size_t>(y) * m_region.size.x() + x] = border ? 1.f : 0.f;
			}
		}
		return result;
	}

	auto scatter(const GraphNode& iNode) -> Points {
		const auto mask = fieldInput(iNode, "Mask");
		std::vector<math::vec2ui> candidates;
		for (uint32_t y = 0; y < m_region.size.y(); ++y) {
			for (uint32_t x = 0; x < m_region.size.x(); ++x) {
				if (mask.get(x, y) >= g_maskThreshold)
					candidates.emplace_back(x, y);
			}
		}
		uint64_t state = nodeSeed(iNode);
		for (size_t i = candidates.size(); i > 1; --i) {
			state = mix(state);
			std::swap(candidates[i - 1], candidates[static_cast<size_t>(state % i)]);
		}
		const float density = std::clamp(number(iNode, "density"), 0.f, 1.f);
		const auto tries = static_cast<size_t>(std::lround(density * static_cast<float>(candidates.size())));
		const auto spacing = std::clamp<int64_t>(integer(iNode, "spacing"), 0, 64);
		std::vector<bool> blocked(m_region.size.surface(), false);
		Points result;
		for (size_t i = 0; i < tries; ++i) {
			const auto& cell = candidates[i];
			if (blocked[static_cast<size_t>(cell.y()) * m_region.size.x() + cell.x()])
				continue;
			result.cells.push_back(cell);
			for (int64_t dy = 1 - spacing; dy < spacing; ++dy) {
				for (int64_t dx = 1 - spacing; dx < spacing; ++dx) {
					const int64_t x = cell.x() + dx;
					const int64_t y = cell.y() + dy;
					if (dx * dx + dy * dy >= spacing * spacing || x < 0 || y < 0 ||
						x >= static_cast<int64_t>(m_region.size.x()) || y >= static_cast<int64_t>(m_region.size.y()))
						continue;
					blocked[static_cast<size_t>(y) * m_region.size.x() + static_cast<size_t>(x)] = true;
				}
			}
		}
		return result;
	}

	[[nodiscard]] auto pointsToMask(const Points& iPoints, const int64_t iRadius) const -> Field {
		auto result = emptyField();
		const auto radius = std::clamp<int64_t>(iRadius, 0, 16);
		for (const auto& cell: iPoints.cells) {
			for (int64_t dy = -radius; dy <= radius; ++dy) {
				for (int64_t dx = -radius; dx <= radius; ++dx) {
					const int64_t x = cell.x() + dx;
					const int64_t y = cell.y() + dy;
					if (x < 0 || y < 0 || x >= static_cast<int64_t>(m_region.size.x()) ||
						y >= static_cast<int64_t>(m_region.size.y()))
						continue;
					result.values[static_cast<size_t>(y) * m_region.size.x() + static_cast<size_t>(x)] = 1.f;
				}
			}
		}
		return result;
	}

	void writeOutput(const GraphNode& iNode) {
		const auto values = m_result.values.find(iNode.id);
		if (values == m_result.values.end())
			return;
		if (iNode.type == "output.height") {
			if (const auto pin = values->second.find("Height"); pin != values->second.end())
				if (const auto* heights = std::get_if<Field>(&pin->second); heights != nullptr)
					m_result.height = *heights;
			return;
		}
		const bool erase = iNode.type == "output.erase";
		const auto tile =
				erase ? int32_t{-1} : static_cast<int32_t>(std::clamp<int64_t>(integer(iNode, "tile"), 0, 65535));
		for (const auto& [pin, value]: values->second) {
			if (const auto* mask = std::get_if<Field>(&value); mask != nullptr) {
				for (size_t i = 0; i < mask->values.size() && i < m_result.tiles.size(); ++i) {
					if (mask->values[i] >= g_maskThreshold)
						m_result.tiles[i] = tile;
				}
			} else if (const auto* cells = std::get_if<Points>(&value); cells != nullptr) {
				for (const auto& cell: cells->cells)
					m_result.tiles[static_cast<size_t>(cell.y()) * m_region.size.x() + cell.x()] = tile;
			}
		}
	}

	const Graph& m_graph;
	Region m_region;
	Result& m_result;
};

}// namespace

auto evaluate(const Graph& iGraph, const Region& iRegion) -> expected<Result, PcgError> {
	OWL_PROFILE_FUNCTION()

	if (iRegion.size.surface() == 0) {
		OWL_CORE_WARN("PcgGraph: The region to generate has no cell.")
		return unexpected{PcgError::EmptyRegion};
	}
	Result result{.size = iRegion.size,
				  .tiles = std::vector<int32_t>(iRegion.size.surface(), g_UntouchedTile),
				  .values = {},
				  .warnings = {},
				  .height = {}};
	if (Evaluator evaluator{iGraph, iRegion, result}; !evaluator.run()) {
		OWL_CORE_WARN("PcgGraph: The graph has a cycle, nothing generated.")
		return unexpected{PcgError::Cycle};
	}
	return result;
}

}// namespace owl::scene::pcg

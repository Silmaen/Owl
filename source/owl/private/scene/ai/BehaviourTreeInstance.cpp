/**
 * @file BehaviourTreeInstance.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ai/BehaviourTreeInstance.h"

#include "core/Macros.h"
#include "math/RandomStream.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "script/ScriptInstance.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace owl::scene::ai {

namespace {

enum struct Op : uint8_t {
	Root,
	Sequence,
	Selector,
	Parallel,
	Random,
	Inverter,
	Succeeder,
	Repeat,
	Retry,
	Cooldown,
	Timeout,
	Guard,
	Compare,
	Distance,
	Chance,
	LuaCondition,
	Wait,
	Set,
	MoveTo,
	Steer,
	FindPlayer,
	LuaAction,
	Log,
	Unknown
};

auto opOf(const std::string_view iType) -> Op {
	static const std::array<std::pair<std::string_view, Op>, 23> s_ops{{{"root", Op::Root},
																		{"composite.sequence", Op::Sequence},
																		{"composite.selector", Op::Selector},
																		{"composite.parallel", Op::Parallel},
																		{"composite.random", Op::Random},
																		{"decorator.inverter", Op::Inverter},
																		{"decorator.succeeder", Op::Succeeder},
																		{"decorator.repeat", Op::Repeat},
																		{"decorator.retry", Op::Retry},
																		{"decorator.cooldown", Op::Cooldown},
																		{"decorator.timeout", Op::Timeout},
																		{"decorator.guard", Op::Guard},
																		{"condition.compare", Op::Compare},
																		{"condition.distance", Op::Distance},
																		{"condition.chance", Op::Chance},
																		{"condition.lua", Op::LuaCondition},
																		{"action.wait", Op::Wait},
																		{"action.set", Op::Set},
																		{"action.move_to", Op::MoveTo},
																		{"action.steer", Op::Steer},
																		{"action.find_player", Op::FindPlayer},
																		{"action.lua", Op::LuaAction},
																		{"action.log", Op::Log}}};
	const auto* const found = std::ranges::find(s_ops, iType, &std::pair<std::string_view, Op>::first);
	return found == s_ops.end() ? Op::Unknown : found->second;
}

auto toFloat(const std::string& iText, const float iDefault) -> float {
	if (const auto value = parseValue(BlackboardType::Number, iText); value.has_value())
		return static_cast<float>(std::get<double>(*value));
	return iDefault;
}

// A node ready to run: its parameters read once.
struct Compiled {
	core::UUID id;
	Op op{Op::Unknown};
	std::vector<uint32_t> children;
	// Number parameters: seconds, distance, weight, probability, spread.
	float number{0.f};
	float second{0.f};
	// Whole-number parameter: count.
	int64_t count{0};
	// Flag parameter: reactive, exclusive.
	bool flag{false};
	// Key of the blackboard read or written.
	std::string key;
	// Text parameter: Lua function, message, value, comparison, policy, behaviour.
	std::string text;
	std::string choice;
};

// The state of a node in one agent.
struct State {
	NodeStatus status{NodeStatus::Idle};
	// Child the composite is at.
	uint32_t cursor{0};
	// Time spent running.
	float timer{0.f};
	// Duration of a wait.
	float duration{0.f};
	// Runs done by a repeat or a retry.
	int64_t count{0};
	// Time from which a cooldown lets its child run again.
	float readyAt{0.f};
	// Goal of a walk.
	math::vec3 goal{0.f, 0.f, 0.f};
	// Order of the children of a random node.
	std::vector<uint32_t> order;
};

auto isTruthy(const BlackboardValue& iValue) -> bool {
	return std::visit(
			[]<typename T>(const T& iHeld) -> bool {
				if constexpr (std::is_same_v<T, bool>)
					return iHeld;
				else if constexpr (std::is_same_v<T, double>)
					return iHeld < 0.0 || iHeld > 0.0;
				else if constexpr (std::is_same_v<T, std::string>)
					return !iHeld.empty();
				else if constexpr (std::is_same_v<T, core::UUID>)
					return iHeld != core::UUID{0};
				else
					return iHeld.normSq() > 0.f;
			},
			iValue);
}

auto compare(const BlackboardValue& iLeft, const std::string& iOp, const std::string& iRight) -> bool {
	if (iOp == "is set")
		return isTruthy(iLeft);
	if (iOp == "is not set")
		return !isTruthy(iLeft);
	const auto right = parseValue(getBlackboardType(iLeft), iRight);
	if (!right.has_value())
		return false;
	if (const auto* number = std::get_if<double>(&iLeft); number != nullptr) {
		const double other = std::get<double>(*right);
		if (iOp == "<")
			return *number < other;
		if (iOp == "<=")
			return *number <= other;
		if (iOp == ">")
			return *number > other;
		if (iOp == ">=")
			return *number >= other;
		if (iOp == "==")
			return !(*number < other) && !(*number > other);
		if (iOp == "!=")
			return *number < other || *number > other;
		return false;
	}

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	if (iOp == "==")
		return iLeft == *right;
	if (iOp == "!=")
		return iLeft != *right;
	OWL_DIAG_POP

	return false;
}

// The script that defines a function: the one of the entity, else the scene script.
auto scriptWith(const Entity& iEntity, const std::string& iName) -> const script::ScriptInstance* {
	if (iEntity.hasComponent<component::LuaScript>()) {
		if (const auto& script = iEntity.getComponent<component::LuaScript>();
			script.instance && script.instance->isValid() && script.instance->hasFunction(iName))
			return script.instance.get();
	}
	if (const auto& script = std::as_const(*iEntity.getScene()).getSceneScript();
		script.instance && script.instance->isValid() && script.instance->hasFunction(iName))
		return script.instance.get();
	return nullptr;
}

auto worldPosition(const Scene& iScene, const Entity& iEntity) -> math::vec3 {
	const math::mat4 world = iScene.getWorldTransform(iEntity)();
	return {world(0, 3), world(1, 3), world(2, 3)};
}

auto findPlayer(Scene& ioScene) -> Entity {
	if (const Entity player = ioScene.getPrimaryPlayer(); player)
		return player;
	if (const auto walkers = ioScene.registry.view<component::FirstPersonController>();
		walkers.begin() != walkers.end())
		return Entity{*walkers.begin(), &ioScene};
	if (const auto voxels = ioScene.registry.view<component::VoxelPlayer>(); voxels.begin() != voxels.end())
		return Entity{*voxels.begin(), &ioScene};
	return {};
}

void clearTargetWeights(component::SteeringWeights& ioWeights) {
	ioWeights.seek = 0.f;
	ioWeights.flee = 0.f;
	ioWeights.arrive = 0.f;
	ioWeights.pursue = 0.f;
	ioWeights.evade = 0.f;
	ioWeights.wander = 0.f;
}

}// namespace

struct BehaviourTreeInstance::Impl {
	shared<const BehaviourTree> tree;
	std::vector<Compiled> nodes;
	std::vector<State> states;
	std::map<std::string, BlackboardValue, std::less<>> blackboard;
	NodeStatus status{NodeStatus::Idle};
	float time{0.f};
	uint64_t ticks{0};
	// The tick being run.
	Entity entity;
	float seconds{0.f};

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root; each node has one parent.
	auto compile(const TreeNode& iNode, std::set<core::UUID>& ioSeen) -> uint32_t {
		const auto index = static_cast<uint32_t>(nodes.size());
		ioSeen.insert(iNode.id);
		Compiled compiled;
		compiled.id = iNode.id;
		compiled.op = opOf(iNode.type);
		const auto value = [&iNode](const std::string_view iParam) -> std::string {
			return BehaviourTree::getValue(iNode, iParam);
		};
		switch (compiled.op) {
			case Op::Selector:
				compiled.flag = value("reactive") == "true";
				break;
			case Op::Parallel:
				compiled.choice = value("policy");
				break;
			case Op::Repeat:
			case Op::Retry:
				compiled.count = static_cast<int64_t>(toFloat(value("count"), 0.f));
				break;
			case Op::Cooldown:
			case Op::Timeout:
				compiled.number = toFloat(value("seconds"), 1.f);
				break;
			case Op::Guard:
			case Op::Compare:
				compiled.key = value("key");
				compiled.choice = value("op");
				compiled.text = value("value");
				break;
			case Op::Distance:
				compiled.key = value("key");
				compiled.choice = value("op");
				compiled.number = toFloat(value("distance"), 5.f);
				compiled.text = value("space");
				break;
			case Op::Chance:
				compiled.number = toFloat(value("probability"), 0.5f);
				break;
			case Op::LuaCondition:
			case Op::LuaAction:
				compiled.text = value("function");
				break;
			case Op::Wait:
				compiled.number = toFloat(value("seconds"), 1.f);
				compiled.second = toFloat(value("spread"), 0.f);
				break;
			case Op::Set:
				compiled.key = value("key");
				compiled.text = value("value");
				break;
			case Op::MoveTo:
				compiled.key = value("key");
				compiled.number = toFloat(value("distance"), 0.5f);
				break;
			case Op::Steer:
				compiled.choice = value("behaviour");
				compiled.key = value("key");
				compiled.number = toFloat(value("weight"), 1.f);
				compiled.flag = value("exclusive") != "false";
				break;
			case Op::FindPlayer:
				compiled.key = value("key");
				break;
			case Op::Log:
				compiled.text = value("message");
				break;
			case Op::Root:
			case Op::Sequence:
			case Op::Random:
			case Op::Inverter:
			case Op::Succeeder:
			case Op::Unknown:
				break;
		}
		nodes.push_back(std::move(compiled));
		states.emplace_back();
		const auto* type = findNodeType(iNode.type);
		const size_t most = type != nullptr ? getMaxChildren(type->kind) : 0;
		std::vector<uint32_t> children;
		for (const auto childId: tree->getChildren(iNode.id)) {
			if (children.size() >= most || ioSeen.contains(childId))
				break;
			if (const auto* child = tree->findNode(childId); child != nullptr)
				children.push_back(compile(*child, ioSeen));
		}
		nodes[index].children = std::move(children);
		return index;
	}

	[[nodiscard]] auto valueOf(const std::string& iKey) const -> BlackboardValue {
		if (const auto found = blackboard.find(iKey); found != blackboard.end())
			return found->second;
		return 0.0;
	}

	// The point held by a key: the position of its entity, or the vector.
	[[nodiscard]] auto pointOf(const std::string& iKey) const -> std::optional<math::vec3> {
		const auto found = blackboard.find(iKey);
		if (found == blackboard.end())
			return std::nullopt;
		if (const auto* point = std::get_if<math::vec3>(&found->second); point != nullptr)
			return *point;
		if (const auto* uuid = std::get_if<core::UUID>(&found->second); uuid != nullptr && *uuid != core::UUID{0}) {
			if (const Entity target = entity.getScene()->findEntityByUUID(*uuid); target)
				return worldPosition(*entity.getScene(), target);
		}
		return std::nullopt;
	}

	[[nodiscard]] auto random() const -> math::RandomStream& {
		return entity.getScene()->getRandomStream("owl.behaviour_trees");
	}

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root.
	void resetSubtree(const uint32_t iIndex) {
		states[iIndex].status = NodeStatus::Idle;
		for (const auto child: nodes[iIndex].children) resetSubtree(child);
	}

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root.
	void haltNode(const uint32_t iIndex) {
		auto& state = states[iIndex];
		if (state.status != NodeStatus::Running)
			return;
		for (const auto child: nodes[iIndex].children) haltNode(child);
		if (nodes[iIndex].op == Op::MoveTo)
			stopWalking();
		state.status = NodeStatus::Idle;
	}

	void stopWalking() const {
		if (entity.hasComponent<component::NavAgent>())
			entity.getComponent<component::NavAgent>().stop();
		if (entity.hasComponent<component::SteeringAgent>())
			entity.getComponent<component::SteeringAgent>().weights.arrive = 0.f;
	}

	void start(const uint32_t iIndex) {
		auto& state = states[iIndex];
		const auto& node = nodes[iIndex];
		state.cursor = 0;
		state.timer = 0.f;
		state.count = 0;
		if (node.op == Op::Parallel) {
			for (const auto child: node.children) resetSubtree(child);
		} else if (node.op == Op::Random) {
			state.order.resize(node.children.size());
			for (uint32_t index = 0; index < state.order.size(); ++index) state.order[index] = index;
			for (size_t index = state.order.size(); index > 1; --index)
				std::swap(state.order[index - 1],
						  state.order[static_cast<size_t>(random().nextInt(0, static_cast<int64_t>(index) - 1))]);
		} else if (node.op == Op::Wait) {
			state.duration = node.number + (node.second > 0.f ? random().nextRange(0.f, node.second) : 0.f);
		}
	}

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root.
	auto run(const uint32_t iIndex) -> NodeStatus {
		if (states[iIndex].status != NodeStatus::Running)
			start(iIndex);
		const NodeStatus result = execute(iIndex);
		states[iIndex].status = result;
		return result;
	}

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root.
	auto execute(const uint32_t iIndex) -> NodeStatus {
		const auto& node = nodes[iIndex];
		auto& state = states[iIndex];
		const auto childCount = static_cast<uint32_t>(node.children.size());
		switch (node.op) {
			case Op::Root:
				return childCount == 0 ? NodeStatus::Failure : run(node.children.front());
			case Op::Sequence:
				for (; state.cursor < childCount; ++state.cursor)
					if (const auto result = run(node.children[state.cursor]); result != NodeStatus::Success)
						return result;
				return NodeStatus::Success;
			case Op::Selector:
				if (node.flag)
					return runReactive(iIndex);
				for (; state.cursor < childCount; ++state.cursor)
					if (const auto result = run(node.children[state.cursor]); result != NodeStatus::Failure)
						return result;
				return NodeStatus::Failure;
			case Op::Random:
				for (; state.cursor < childCount; ++state.cursor)
					if (const auto result = run(node.children[state.order[state.cursor]]);
						result != NodeStatus::Failure)
						return result;
				return NodeStatus::Failure;
			case Op::Parallel:
				return runParallel(iIndex);
			case Op::Inverter:
				{
					const auto result = childCount == 0 ? NodeStatus::Failure : run(node.children.front());
					if (result == NodeStatus::Running)
						return result;
					return result == NodeStatus::Success ? NodeStatus::Failure : NodeStatus::Success;
				}
			case Op::Succeeder:
				{
					const auto result = childCount == 0 ? NodeStatus::Success : run(node.children.front());
					return result == NodeStatus::Running ? result : NodeStatus::Success;
				}
			case Op::Repeat:
			case Op::Retry:
				{
					if (childCount == 0)
						return NodeStatus::Failure;
					const NodeStatus again = node.op == Op::Repeat ? NodeStatus::Success : NodeStatus::Failure;
					const auto result = run(node.children.front());
					if (result == NodeStatus::Running || result != again)
						return result;
					++state.count;
					return node.count > 0 && state.count >= node.count ? result : NodeStatus::Running;
				}
			case Op::Cooldown:
				{
					if (time < state.readyAt || childCount == 0)
						return NodeStatus::Failure;
					const auto result = run(node.children.front());
					if (result != NodeStatus::Running)
						state.readyAt = time + node.number;
					return result;
				}
			case Op::Timeout:
				state.timer += seconds;
				if (childCount == 0)
					return NodeStatus::Failure;
				if (state.timer > node.number) {
					haltNode(node.children.front());
					return NodeStatus::Failure;
				}
				return run(node.children.front());
			case Op::Guard:
				if (childCount == 0)
					return NodeStatus::Failure;
				if (!compare(valueOf(node.key), node.choice, node.text)) {
					haltNode(node.children.front());
					return NodeStatus::Failure;
				}
				return run(node.children.front());
			case Op::Compare:
				return compare(valueOf(node.key), node.choice, node.text) ? NodeStatus::Success : NodeStatus::Failure;
			case Op::Distance:
				{
					const auto point = pointOf(node.key);
					if (!point.has_value())
						return NodeStatus::Failure;
					math::vec3 offset = *point - worldPosition(*entity.getScene(), entity);
					if (node.text == "ground")
						offset.y() = 0.f;
					else if (node.text == "plane")
						offset.z() = 0.f;
					const float distance = offset.norm();
					const bool closer = distance < node.number;
					return closer == (node.choice != "farther than") ? NodeStatus::Success : NodeStatus::Failure;
				}
			case Op::Chance:
				return random().nextFloat() < node.number ? NodeStatus::Success : NodeStatus::Failure;
			case Op::LuaCondition:
			case Op::LuaAction:
				return callLua(node);
			case Op::Wait:
				state.timer += seconds;
				return state.timer >= state.duration ? NodeStatus::Success : NodeStatus::Running;
			case Op::Set:
				{
					const auto* key = tree->findKey(node.key);
					const auto type = key != nullptr ? getBlackboardType(key->value) : BlackboardType::String;
					const auto parsed = parseValue(type, node.text);
					if (!parsed.has_value())
						return NodeStatus::Failure;
					blackboard[node.key] = *parsed;
					return NodeStatus::Success;
				}
			case Op::MoveTo:
				return moveTo(iIndex);
			case Op::Steer:
				return steer(node);
			case Op::FindPlayer:
				if (const Entity player = findPlayer(*entity.getScene()); player) {
					blackboard[node.key] = player.getUUID();
					return NodeStatus::Success;
				}
				return NodeStatus::Failure;
			case Op::Log:
				OWL_CORE_INFO("BehaviourTree: {} says: {}.", entity.getName(), node.text)
				return NodeStatus::Success;
			case Op::Unknown:
				break;
		}
		return NodeStatus::Failure;
	}

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root.
	auto runReactive(const uint32_t iIndex) -> NodeStatus {
		const auto& node = nodes[iIndex];
		auto& state = states[iIndex];
		for (uint32_t index = 0; index < node.children.size(); ++index) {
			const auto result = run(node.children[index]);
			if (result == NodeStatus::Failure)
				continue;
			if (state.cursor != index && state.cursor < node.children.size())
				haltNode(node.children[state.cursor]);
			state.cursor = index;
			return result;
		}
		return NodeStatus::Failure;
	}

	// NOLINTNEXTLINE(misc-no-recursion) A tree is walked from its root.
	auto runParallel(const uint32_t iIndex) -> NodeStatus {
		const auto& node = nodes[iIndex];
		const bool needAll = node.choice != "one";
		size_t successes = 0;
		size_t failures = 0;
		for (const auto child: node.children) {
			auto result = states[child].status;
			if (result == NodeStatus::Idle || result == NodeStatus::Running)
				result = run(child);
			if (result == NodeStatus::Success)
				++successes;
			else if (result == NodeStatus::Failure)
				++failures;
		}
		const size_t count = node.children.size();
		NodeStatus outcome = NodeStatus::Running;
		if (needAll ? failures > 0 : failures == count)
			outcome = NodeStatus::Failure;
		else if (needAll ? successes == count : successes > 0)
			outcome = NodeStatus::Success;
		if (outcome != NodeStatus::Running)
			for (const auto child: node.children) haltNode(child);
		return outcome;
	}

	[[nodiscard]] auto callLua(const Compiled& iNode) const -> NodeStatus {
		const auto* script = scriptWith(entity, iNode.text);
		if (script == nullptr) {
			OWL_CORE_WARN_ONCE("BehaviourTree: '{}' finds no Lua function '{}'.", entity.getName(), iNode.text)
			return NodeStatus::Failure;
		}
		const auto value = script->callFunctionForValue(iNode.text, static_cast<uint64_t>(entity.getUUID()), seconds);
		if (!value.has_value())
			return NodeStatus::Failure;
		if (iNode.op == Op::LuaCondition)
			return *value == "nil" || *value == "false" ? NodeStatus::Failure : NodeStatus::Success;
		if (*value == "running")
			return NodeStatus::Running;
		return *value == "true" || *value == "nil" ? NodeStatus::Success : NodeStatus::Failure;
	}

	auto moveTo(const uint32_t iIndex) -> NodeStatus {
		const auto& node = nodes[iIndex];
		auto& state = states[iIndex];
		const auto point = pointOf(node.key);
		if (!point.has_value())
			return NodeStatus::Failure;
		const math::vec3 here = worldPosition(*entity.getScene(), entity);
		const bool started = state.status != NodeStatus::Running;
		if ((*point - here).norm() <= node.number) {
			stopWalking();
			return NodeStatus::Success;
		}
		if (entity.hasComponent<component::NavAgent>()) {
			auto& agent = entity.getComponent<component::NavAgent>();
			// A moving goal is followed: the path is found again when it moved farther than the arrival distance.
			if (started || (*point - state.goal).norm() > std::max(node.number, 0.25f)) {
				agent.goTo(*point);
				state.goal = *point;
				return NodeStatus::Running;
			}
			return agent.isMoving() ? NodeStatus::Running : NodeStatus::Failure;
		}
		if (entity.hasComponent<component::SteeringAgent>()) {
			auto& agent = entity.getComponent<component::SteeringAgent>();
			if (started)
				clearTargetWeights(agent.weights);
			agent.weights.arrive = 1.f;
			const auto& held = blackboard.find(node.key)->second;
			if (const auto* uuid = std::get_if<core::UUID>(&held); uuid != nullptr)
				agent.target = *uuid;
			else {
				agent.target = core::UUID{0};
				agent.targetPoint = *point;
			}
			return NodeStatus::Running;
		}
		OWL_CORE_WARN_ONCE("BehaviourTree: '{}' moves with neither a Nav Agent nor a Steering Agent.", entity.getName())
		return NodeStatus::Failure;
	}

	auto steer(const Compiled& iNode) -> NodeStatus {
		if (!entity.hasComponent<component::SteeringAgent>()) {
			OWL_CORE_WARN_ONCE("BehaviourTree: '{}' steers without a Steering Agent.", entity.getName())
			return NodeStatus::Failure;
		}
		auto& agent = entity.getComponent<component::SteeringAgent>();
		if (iNode.choice == "stop") {
			clearTargetWeights(agent.weights);
			return NodeStatus::Success;
		}
		float* weight = agent.weights.find(iNode.choice);
		if (weight == nullptr)
			return NodeStatus::Failure;
		if (!iNode.key.empty()) {
			const auto found = blackboard.find(iNode.key);
			if (found == blackboard.end())
				return NodeStatus::Failure;
			if (const auto* uuid = std::get_if<core::UUID>(&found->second); uuid != nullptr)
				agent.target = *uuid;
			else if (const auto* point = std::get_if<math::vec3>(&found->second); point != nullptr) {
				agent.target = core::UUID{0};
				agent.targetPoint = *point;
			} else
				return NodeStatus::Failure;
		}
		if (iNode.flag)
			clearTargetWeights(agent.weights);
		*weight = iNode.number;
		return NodeStatus::Success;
	}
};

BehaviourTreeInstance::BehaviourTreeInstance(shared<const BehaviourTree> iTree,
											 const std::span<const BlackboardKey> iOverrides)
	: mp_impl{mkUniq<Impl>()} {
	mp_impl->tree = std::move(iTree);
	if (!mp_impl->tree)
		return;
	for (const auto& key: mp_impl->tree->blackboard) mp_impl->blackboard[key.name] = key.value;
	for (const auto& key: iOverrides) {
		if (const auto found = mp_impl->blackboard.find(key.name);
			found != mp_impl->blackboard.end() && found->second.index() == key.value.index())
			found->second = key.value;
	}
	if (const auto* root = mp_impl->tree->getRoot(); root != nullptr) {
		std::set<core::UUID> seen;
		std::ignore = mp_impl->compile(*root, seen);
	}
}

BehaviourTreeInstance::~BehaviourTreeInstance() = default;

auto BehaviourTreeInstance::tick(const Entity& iEntity, const float iSeconds) -> NodeStatus {
	auto& impl = *mp_impl;
	if (impl.nodes.empty() || !iEntity)
		return NodeStatus::Failure;
	impl.entity = iEntity;
	impl.seconds = iSeconds;
	impl.time += iSeconds;
	++impl.ticks;
	impl.status = impl.run(0);
	return impl.status;
}

void BehaviourTreeInstance::halt(const Entity& iEntity) {
	auto& impl = *mp_impl;
	if (impl.nodes.empty() || !iEntity)
		return;
	impl.entity = iEntity;
	impl.haltNode(0);
	impl.resetSubtree(0);
	impl.status = NodeStatus::Idle;
}

auto BehaviourTreeInstance::getStatus() const -> NodeStatus { return mp_impl->status; }

auto BehaviourTreeInstance::getNodeStatus(const core::UUID iNode) const -> NodeStatus {
	for (size_t index = 0; index < mp_impl->nodes.size(); ++index)
		if (mp_impl->nodes[index].id == iNode)
			return mp_impl->states[index].status;
	return NodeStatus::Idle;
}

auto BehaviourTreeInstance::getBlackboard() const -> const std::map<std::string, BlackboardValue, std::less<>>& {
	return mp_impl->blackboard;
}

auto BehaviourTreeInstance::getValue(const std::string_view iKey) const -> const BlackboardValue* {
	const auto found = mp_impl->blackboard.find(iKey);
	return found == mp_impl->blackboard.end() ? nullptr : &found->second;
}

void BehaviourTreeInstance::setValue(const std::string_view iKey, BlackboardValue iValue) {
	mp_impl->blackboard.insert_or_assign(std::string{iKey}, std::move(iValue));
}

auto BehaviourTreeInstance::getTree() const -> const shared<const BehaviourTree>& { return mp_impl->tree; }

auto BehaviourTreeInstance::getTickCount() const -> uint64_t { return mp_impl->ticks; }

}// namespace owl::scene::ai

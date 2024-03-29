#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <utility>
#include <vector>

#include "ankerl/unordered_dense.h"
#include "Common.h"
#include "Model.h"

template<uint N>
using QueryResultsType = std::vector<std::tuple<int, GridState<N>, float>>;

template<uint N>
class ITablebase{
public:
	ITablebase(float fourChance) : m_fourChance(fourChance) {}
	virtual ~ITablebase() {}
	virtual void init(const GridState<N>& initState=GridState<N>(), int maxDepth = -1) = 0;
	virtual float query(const GridState<N>& state) const = 0;
	virtual void recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const = 0;
protected:
	const float m_fourChance;
};

template<uint N>
class InMemoryTablebase : public ITablebase<N> {
public:
	InMemoryTablebase(float fourChance) : ITablebase<N>(fourChance) {}
	virtual void init(const GridState<N>& initState=GridState<N>(), int maxDepth = -1) override;
	virtual float query(const GridState<N>& state) const override;
	virtual void recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const override;
	void dump(std::ostream &o) const;
private:
	void generateEdges(const GridState<N>& state, bool intermediate, int maxDepth, int depth);
	void calculateScores(const GridState<N>& state, bool intermediate);
	ankerl::unordered_dense::map<GridState<N>, std::pair<float,float> /*final_score, intermediate_score*/> m_nodes;
	ankerl::unordered_dense::map<GridState<N>, ankerl::unordered_dense::map<GridState<N>, float>> m_edges;
	ankerl::unordered_dense::set<std::pair<GridState<N>, bool>> m_edgesGenerated;
	ankerl::unordered_dense::set<std::pair<GridState<N>, bool>> m_calculatedScore;
};

template<uint N>
void InMemoryTablebase<N>::init(const GridState<N>& initState, int maxDepth) {
	m_nodes[initState] = std::make_pair(-1.0f, -1.0f);
	generateEdges(initState, true, maxDepth, 0);
	calculateScores(initState, true);
	DEBUG_LOG("node count: " << m_nodes.size() << " edge count: " << m_edges.size() << std::endl);
}

template<uint N>
float InMemoryTablebase<N>::query(const GridState<N>& state) const {
	auto it = m_nodes.find(state);
	return it == m_nodes.end() ? -1.0f : it->second.first;
}

template<uint N>
void InMemoryTablebase<N>::recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const {
	if(currDepth > maxDepth) return;
	bool intermediate = currDepth & 1;
	results.emplace_back(std::make_tuple(currDepth, state, intermediate ? m_nodes.at(state).second : m_nodes.at(state).first));
	auto it = m_edges.find(state);
	if(it != m_edges.end()) {
		for(const auto &p : it->second) {
			const GridState<N>& childState = p.first;
			float edgeWeight = p.second;
			if((!intermediate && edgeWeight != -1.0f) || (intermediate && edgeWeight == -1.0f)) continue;
			recursiveQuery(childState, currDepth + 1, maxDepth, results);
		}
	}
}

template<uint N>
void InMemoryTablebase<N>::generateEdges(const GridState<N>& state, bool intermediate, int maxDepth, int depth) {
	if((maxDepth >= 0 && depth > maxDepth) || m_edgesGenerated.count(std::make_pair(state, intermediate))) return;
	m_edgesGenerated.insert(std::make_pair(state, intermediate));
	// Node is being treated as an intermediate node: next action is random tile spawn
	if(intermediate) {
		uint emptyTiles = state.numEmptyTiles();
		for(uint r = 0; r < N; ++r) {
			for(uint c = 0; c < N; ++c) {
				if(!state.isEmpty(r,c)) continue;

				for(uint i = 0; i < 2; ++i) {
					GridState<N> child = state;
					child.writeTile(r, c, i + 1);
					auto it = m_nodes.find(child);
					if(it == m_nodes.end()) {
						m_nodes.insert(it, std::make_pair(child, std::make_pair(-1.0f, -1.0f)));
					}
					m_edges[state][child] = (i ? ITablebase<N>::m_fourChance : (1.0f - ITablebase<N>::m_fourChance)) / emptyTiles;
					generateEdges(child, !intermediate, maxDepth, depth);
				}
			}
		}
	}
	// Node is being treated as an real node: next action is user swipe 
	else {
		std::array<GridState<N>, 4> childStates = {state, state, state, state};
		childStates[0].swipe(0, -1);
		childStates[1].swipe(0, 1);
		childStates[2].swipe(-1, 0);
		childStates[3].swipe(1, 0);
		for(uint i=0; i < 4; ++i) {
			if(childStates[i] == state) continue;
			auto it = m_nodes.find(childStates[i]);
			if(it == m_nodes.end()) {
				m_nodes.insert(it, std::make_pair(childStates[i], std::make_pair(-1.0f, -1.0f)));
			}
			m_edges[state][childStates[i]] = -1;
			generateEdges(childStates[i], !intermediate, maxDepth, depth + 1);
		}
	}
}

template<uint N>
void InMemoryTablebase<N>::calculateScores(const GridState<N>& state, bool intermediate) {
	// Already Calculated
	if(m_calculatedScore.count(std::make_pair(state, intermediate))) return;
	m_calculatedScore.insert(std::make_pair(state, intermediate));
	DEBUG_LOG("Begin ");
	DEBUG_EXEC(state.printCompact(std::cerr));
	DEBUG_LOG("-" << intermediate << std::endl);

	// Meets the win condition of having the max tile
	// In the future this should be updated to handle alternate win conditions
	if(state.hasTile(N*N + 1)) {
		m_nodes[state] = std::make_pair(1.0f, 1.0f);
		DEBUG_LOG("End ");
		DEBUG_EXEC(state.printCompact(std::cerr));
		DEBUG_LOG("-" << intermediate << std::endl);
		return;
	}
	// Lost State
	if(!state.hasMoves() && state != GridState<N>()) {
		DEBUG_LOG("End ");
		DEBUG_EXEC(state.printCompact(std::cerr));
		DEBUG_LOG("-" << intermediate << std::endl);
		m_nodes[state] = std::make_pair(0.0f, 0.0f);
		return;
	}
	// Assume any non-final edge node has a score of 0.5
	// In the future this will use an analysis algorithm to make a better guess
	if(!m_edges.count(state) || m_edges[state].empty()) {
		DEBUG_LOG("BADBADBADBADBADBADBADBADBADBADBADBADBADBADB");
		m_nodes[state] = std::make_pair(0.5f, 0.5f);
		return;
	}

	if(intermediate) {
		float score = 0;
		for(const auto &p : m_edges[state]) {
			auto& [childState, edgeWeight] = p;
			calculateScores(childState, !intermediate);
			if(edgeWeight != -1) {
				DEBUG_LOG("real state contribution: ");
				DEBUG_EXEC(state.printCompact(std::cerr));
				DEBUG_LOG(" -> ");
				DEBUG_EXEC(childState.printCompact(std::cerr));
				DEBUG_LOG(": " << edgeWeight * m_nodes[childState].first << std::endl);
				score += edgeWeight * m_nodes[childState].first;
			}
		}
		DEBUG_LOG("calculated intermediate score: ");
		DEBUG_EXEC(state.printCompact(std::cerr));
		DEBUG_LOG(": " << score << std::endl);
		m_nodes[state].second = score;
	}
	else {
		float score = 0;
		for(const auto &p : m_edges[state]) {
			auto& [childState, edgeWeight] = p;
			calculateScores(childState, !intermediate);
			if(edgeWeight == -1) {
				DEBUG_LOG("potential final score: ");
				DEBUG_EXEC(state.printCompact(std::cerr));
				DEBUG_LOG(" -> ");
				DEBUG_EXEC(childState.printCompact(std::cerr));
				DEBUG_LOG(": " << m_nodes[childState].second << std::endl);
				score = std::max(score, m_nodes[childState].second);
			} 
		}
		DEBUG_LOG("calculated final score: ");
		DEBUG_EXEC(state.printCompact(std::cerr));
		DEBUG_LOG(": " << score << std::endl );
		m_nodes[state].first = score;
	}
	DEBUG_LOG("End ");
	DEBUG_EXEC(state.printCompact(std::cerr));
	DEBUG_LOG("-" << intermediate << std::endl);
}

template<uint N>
void InMemoryTablebase<N>::dump(std::ostream &o) const {
	GridState<N> state;
	for(;;) {
		state.printCompact(o);
		o << ": ";
		if(m_nodes.count(state)) {
			o << m_nodes.at(state).first << "," << m_nodes.at(state).second;
		} else {
			o << "-1,-1";
		}
		o << std::endl;

		for(uint i = 0;; ++i) {
			if(i == N*N) return;
			uint tile = state.readTile(i/N, i%N) + 1;
			if(tile == N*N+2) {
				state.writeTile(i/N, i%N, 0);
			}
			else {
				state.writeTile(i/N, i%N, tile);
				break;
			}
		}
	}
}

template<uint N>
inline void printQueryResults(std::ostream& o, const QueryResultsType<N>& results) {
	for(const auto &t: results) {
		auto &[depth, state, score] = t;
		for(int i = 0; i < depth; ++i) o << ' ';
		state.printCompact(o);
		o << ": " << score << std::endl;
	}
}

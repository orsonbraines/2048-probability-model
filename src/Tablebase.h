#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iostream>
#include <utility>
#include <vector>

#include "ankerl/unordered_dense.h"
#include "Common.h"
#include "Model.h"
#include "sqlite3.h"

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
	virtual std::pair<int, int> bestMove(const GridState<N>& state) const = 0;
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
	virtual std::pair<int, int> bestMove(const GridState<N>& state) const override;
	void dump(std::ostream &o) const;
private:
	void generateEdges(const GridState<N>& initState, int maxDepth);
	void calculateScores();
	ankerl::unordered_dense::map<GridState<N>, std::pair<float,float> /*final_score, intermediate_score*/> m_nodes;
	ankerl::unordered_dense::map<GridState<N>, ankerl::unordered_dense::map<GridState<N>, float>> m_edges;
	ankerl::unordered_dense::map<GridState<N>, std::vector<GridState<N>>> m_reverseEdges;
	ankerl::unordered_dense::set<std::pair<GridState<N>, bool>> m_edgesGenerated;
};

template<uint N>
void InMemoryTablebase<N>::init(const GridState<N>& initState, int maxDepth) {
	//m_nodes[initState] = std::make_pair(-1.0f, -1.0f);
	generateEdges(initState, maxDepth);
	calculateScores();
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
void InMemoryTablebase<N>::generateEdges(const GridState<N>& initState, int maxDepth) {
	std::deque<std::pair<GridState<N>, int>> stateQueue;
	stateQueue.push_back(std::make_pair(initState, 0));

	while (!stateQueue.empty()) {
		auto [state, depth] = stateQueue.front();
		stateQueue.pop_front();

		m_nodes[state] = std::make_pair(-1.0f, -1.0f);

		// intermedate edges
		uint emptyTiles = state.numEmptyTiles();
		for(uint r = 0; r < N; ++r) {
			for(uint c = 0; c < N; ++c) {
				if(!state.isEmpty(r,c)) continue;

				for(uint i = 0; i < 2; ++i) {
					GridState<N> child = state;
					child.writeTile(r, c, i + 1);
					m_edges[state][child] = (i ? ITablebase<N>::m_fourChance : (1.0f - ITablebase<N>::m_fourChance)) / emptyTiles;
					m_reverseEdges[child].push_back(state);
					if ((maxDepth < 0 || depth < maxDepth) && m_nodes.count(child) == 0) {
						stateQueue.emplace_back(child, depth + 1);
					}
				}
			}
		}

		// non-intermediate edges
		for(uint i=0; i < 4; ++i) {
			GridState<N> child = state;
			switch (i) {
			case 0: child.swipe(0, -1); break;
			case 1: child.swipe(0, 1); break;
			case 2: child.swipe(-1, 0); break;
			case 3: child.swipe(1, 0); break;
			}

			if (child == state) continue;

			m_edges[state][child] = -1;
			m_reverseEdges[child].push_back(state);
			if ((maxDepth < 0 || depth < maxDepth) && m_nodes.count(child) == 0) {
				stateQueue.emplace_back(child, depth + 1);
			}
		}
	}
}

// We need to be careful about howe we traverse the graph here.
// A DFS will run into dependency loop problems, for example
// -- is an intermediate child of both -- and --
// -4                                  4-     -4
// Which causes us to try to get the score a second time before we're finished the first time.
// To avoid these problems we can do a reverse BFS
template<uint N>
void InMemoryTablebase<N>::calculateScores() {
	std::deque<GridState<N>> stateQueue;
	// Calculate all the trivial scores
	for(const auto& p : m_nodes) {
		const GridState<N>& state = p.first;
		bool foundScore = false;
		//Meets the win condition of having the max tile
		// In the future this should be updated to handle alternate win conditions
		if(state.hasTile(N*N + 1)) {
			m_nodes[state] = std::make_pair(1.0f, 1.0f);
			foundScore = true;
		}
		// Lost State
		else if(!state.hasMoves() && state != GridState<N>()) {
			m_nodes[state] = std::make_pair(0.0f, 0.0f);
			foundScore = true;
		}
		// Assume any non-final edge node has a score of 0.5
		// In the future this will use an analysis algorithm to make a better guess
		else if(!m_edges.count(state)) {
			DEBUG_LOG("I thought me were generating to infinite depth....");
			DEBUG_ASSERT(0);
			m_nodes[state] = std::make_pair(0.5f, 0.5f);
			foundScore = true;
		}

		if(foundScore) {
			auto it = m_reverseEdges.find(state);
			if(it != m_reverseEdges.end()) {
				stateQueue.insert(stateQueue.end(), it->second.begin(), it->second.end());
			}
		}
	}

	while (!stateQueue.empty()) {
		GridState state = stateQueue.front();
		stateQueue.pop_front();
		auto [scoreFinal, scoreInter] = m_nodes[state];
		if (scoreFinal != -1.0f && scoreInter != -1.0f) continue;

		bool foundScore = false;
		if (scoreFinal == -1.0f) {
			// check if all the dependent intermediate scores have been calculated
			bool readyToCalculate = true;
			float score = 0.0f;
			for (const auto& p : m_edges[state]) {
				auto& [childState, edgeWeight] = p;
				if(edgeWeight == -1) {
					if (m_nodes[childState].second == -1) {
						readyToCalculate = false;
						break;
					}
					score = std::max(score, m_nodes[childState].second);
				}
			}
			if (readyToCalculate) {
				m_nodes[state].first = score;
				foundScore = true;
			}
		}
		if (scoreInter == -1.0f) {
			// check if all the dependent intermediate scores have been calculated
			bool readyToCalculate = true;
			float score = 0.0f;
			for (const auto& p : m_edges[state]) {
				auto& [childState, edgeWeight] = p;
				if (edgeWeight != -1) {
					if (m_nodes[childState].first == -1) {
						readyToCalculate = false;
						break;
					}
					score += edgeWeight * m_nodes[childState].first;
				}
			}
			if (readyToCalculate) {
				m_nodes[state].second = score;
				foundScore = true;
			}
		}
		if (foundScore) {
			auto it = m_reverseEdges.find(state);
			if (it != m_reverseEdges.end()) {
				stateQueue.insert(stateQueue.end(), it->second.begin(), it->second.end());
			}
		}
	}
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
std::pair<int, int> InMemoryTablebase<N>::bestMove(const GridState<N>& state) const {
	float bestScore = 0;
	GridState<N> bestState;
	auto it = m_edges.find(state);
	if (it != m_edges.end()) {
		const ankerl::unordered_dense::map<GridState<N>, float >& edges = it->second;
		// find best intermediate child score
		for (const auto& p : edges) {
			if (p.second != -1) continue;
			const GridState<N>& endState = p.first;
			float score = m_nodes.at(endState).second;
			if (score > bestScore) {
				bestScore = score;
				bestState = endState;
			}
		}
	}
	for (uint i = 0; i < 4; ++i) {
		GridState<N> stateClone = state;
		std::pair<int, int> swipeMove;
		switch (i) {
		case 0: swipeMove = std::make_pair(0, -1); break;
		case 1: swipeMove = std::make_pair(0, 1); break;
		case 2: swipeMove = std::make_pair(-1, 0); break;
		case 3: swipeMove = std::make_pair(1, 0); break;
		}
		stateClone.swipe(swipeMove.first, swipeMove.second);
		if (stateClone == bestState) {
			return swipeMove;
		}
	}
	return std::make_pair(-1, -1);
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

template<uint N>
class EmpiricalTablebase {
public:
	void addResult(const std::vector<GridState<N>>& states, bool win);
	double query(const GridState<N>& state) const;
	void compare(const ITablebase<N>& table, std::ostream& o, std::ostream& debug) const;
private:
	ankerl::unordered_dense::map<GridState<N>, std::pair<uint64, uint64>> m_resultMap;
};

template<uint N>
void EmpiricalTablebase<N>::addResult(const std::vector<GridState<N>>& states, bool win) {
	for (const auto& s : states) {
		if (win) ++m_resultMap[s].first;
		else ++m_resultMap[s].second;
	}
}

template<uint N>
double EmpiricalTablebase<N>::query(const GridState<N>& state) const {
	auto it = m_resultMap.find(state);
	if (it != m_resultMap.end()) {
		double wins = it->second.first;
		double total = wins + it->second.second;
		return wins / total;
	}
	return -1.0;
}

template<uint N>
void EmpiricalTablebase<N>::compare(const ITablebase<N>& table, std::ostream& o, std::ostream& debug) const {
	double meanSquare = 0;
	double largestDiff = 0;
	for (const auto& p : m_resultMap) {
		const GridState<N>& state = p.first;
		double ours = query(state);
		double theirs = table.query(state);
		double diff2 = (ours - theirs) * (ours - theirs);
		meanSquare += diff2;
		largestDiff = std::max(largestDiff, diff2);
		state.printCompact(debug);
		debug << " ours: " << ours << " theirs: " << theirs << " diff2: " << diff2 << std::endl;
	}
	meanSquare /= m_resultMap.size();
	o << "rms: " << std::sqrt(meanSquare) << " max(Diff^2): " << largestDiff << std::endl;
}

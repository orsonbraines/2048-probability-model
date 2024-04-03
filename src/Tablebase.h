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
	ITablebase(float fourChance) : m_fourChance(fourChance), m_actionCount{0} {}
	virtual ~ITablebase() {}
	virtual void init(uint64 maxActions = UINT64_MAX, int maxDepth = -1);
	virtual float query(const GridState<N>& state) const = 0;
	virtual void recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const = 0;
	virtual std::pair<int, int> bestMove(const GridState<N>& state) const = 0;
protected:
	virtual void generateEdges(uint64 maxActions, int maxDepth);
	virtual void calculateScores(uint64 maxActions);
	virtual void setNode(const GridState<N>& node, float noninterScore = -1, float interScore = -1) = 0;
	virtual bool hasNode(const GridState<N>& node) = 0;
	virtual std::pair<float, float> getNodeScores(const GridState<N>& node) = 0;
	virtual void pushToEdgeQueue(const GridState<N>& node, int depth) = 0;
	virtual std::pair<GridState<N>, int> popFromEdgeQueue() = 0;
	virtual bool edgeQueueEmpty() = 0;
	virtual void addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) = 0;
	virtual bool hasEdge(const GridState<N>& node) = 0;
	virtual std::vector<GridState<N>> getEdges(const GridState<N>& node) = 0;
	virtual float getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) = 0;
	virtual std::vector<GridState<N>> getParents(const GridState<N>& child) = 0;
	virtual void copyNodesToScoreQueue() = 0;
	virtual void pushToScoreQueue(const GridState<N>& node) = 0;
	virtual GridState<N> popFromScoreQueue() = 0;
	virtual bool scoreQueueEmpty() = 0;
	virtual void addInterScore(const GridState<N>& node, float score) = 0;
	virtual void addNonInterScore(const GridState<N>& node, float score) = 0;
	const float m_fourChance;
	uint64 m_actionCount;
};

template<uint N>
void ITablebase<N>::init(uint64 maxActions, int maxDepth) {
	generateEdges(maxActions, maxDepth);
	calculateScores(maxActions);
}

template<uint N>
void ITablebase<N>::generateEdges(uint64 maxActions, int maxDepth) {
	setNode(GridState<N>());
	pushToEdgeQueue(GridState<N>(), 0);

	for (; m_actionCount < maxActions && !edgeQueueEmpty(); ++m_actionCount) {
		auto [state, depth] = popFromEdgeQueue();

		// intermedate edges
		uint emptyTiles = state.numEmptyTiles();
		for (uint r = 0; r < N; ++r) {
			for (uint c = 0; c < N; ++c) {
				if (!state.isEmpty(r, c)) continue;

				for (uint i = 0; i < 2; ++i) {
					GridState<N> child = state;
					child.writeTile(r, c, i + 1);
					addEdge(state, child, (i ? m_fourChance : (1.0f - m_fourChance)) / emptyTiles);
					if ((maxDepth < 0 || depth < maxDepth) && !hasNode(child)) {
						setNode(child);
						pushToEdgeQueue(child, depth + 1);
					}
				}
			}
		}

		// non-intermediate edges
		for (uint i = 0; i < 4; ++i) {
			GridState<N> child = state;
			switch (i) {
			case 0: child.swipe(0, -1); break;
			case 1: child.swipe(0, 1); break;
			case 2: child.swipe(-1, 0); break;
			case 3: child.swipe(1, 0); break;
			}

			if (child == state) continue;

			addEdge(state, child, -1);
			if ((maxDepth < 0 || depth < maxDepth) && !hasNode(child)) {
				setNode(child);
				pushToEdgeQueue(child, depth + 1);
			}
		}
	}

	DEBUG_LOG("generateEdges exiting after " << m_actionCount << " actions" << std::endl);
	m_actionCount = 0;
}

template<uint N>
void ITablebase<N>::calculateScores(uint64 maxActions) {
	copyNodesToScoreQueue();

	for (; m_actionCount < maxActions && !scoreQueueEmpty(); ++m_actionCount) {
		GridState state = popFromScoreQueue();
		auto [scoreFinal, scoreInter] = getNodeScores(state);
		if (scoreFinal != -1.0f && scoreInter != -1.0f) continue;

		bool foundScore = false;
		// Meets the win condition of having the max tile
		// In the future this should be updated to handle alternate win conditions
		if (state.hasTile(N * N + 1)) {
			setNode(state, 1.0f, 1.0f);
			foundScore = true;
		}
		// Lost State
		else if (!state.hasMoves() && state != GridState<N>()) {
			setNode(state, 0.0f, 0.0f);
			foundScore = true;
		}
		// Assume any non-final edge node has a score of 0.5
		// In the future this will use an analysis algorithm to make a better guess
		else if (!hasEdge(state)) {
			DEBUG_LOG("I thought me were generating to infinite depth....");
			DEBUG_ASSERT(0);
			setNode(state, 0.5f, 0.5f);
			foundScore = true;
		}
		else {
			if (scoreFinal == -1.0f) {
				// check if all the dependent intermediate scores have been calculated
				bool readyToCalculate = true;
				float score = 0.0f;
				for (const GridState<N>& childState : getEdges(state)) {
					float edgeWeight = getEdgeWeight(state, childState);
					if (edgeWeight == -1) {
						if (getNodeScores(childState).second == -1) {
							readyToCalculate = false;
							break;
						}
						score = std::max(score, getNodeScores(childState).second);
					}
				}
				if (readyToCalculate) {
					addNonInterScore(state, score);
					foundScore = true;
				}
			}
			if (scoreInter == -1.0f) {
				// check if all the dependent intermediate scores have been calculated
				bool readyToCalculate = true;
				float score = 0.0f;
				for (const GridState<N>& childState : getEdges(state)) {
					float edgeWeight = getEdgeWeight(state, childState);
					if (edgeWeight != -1) {
						if (getNodeScores(childState).first == -1) {
							readyToCalculate = false;
							break;
						}
						score += edgeWeight * getNodeScores(childState).first;
					}
				}
				if (readyToCalculate) {
					addInterScore(state, score);
					foundScore = true;
				}
			}
		}
		if (foundScore) {
			for (const GridState<N>& prevState : getParents(state)) {
				auto [prevScoreFinal, prevScoreInter] = getNodeScores(prevState);
				if (prevScoreFinal == -1.0f || prevScoreInter == -1.0f) {
					pushToScoreQueue(prevState);
				}
			}
		}
	}
	DEBUG_LOG("calculateScores exiting after " << m_actionCount << " actions" << std::endl);
	m_actionCount = 0;
}

template<uint N>
class InMemoryTablebase : public ITablebase<N> {
public:
	InMemoryTablebase(float fourChance) : ITablebase<N>(fourChance) {}
	virtual void init(uint64 maxActions = UINT64_MAX, int maxDepth = -1) override;
	virtual float query(const GridState<N>& state) const override;
	virtual void recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const override;
	virtual std::pair<int, int> bestMove(const GridState<N>& state) const override;
	void dump(std::ostream &o) const;
protected:
	virtual void setNode(const GridState<N>& node, float noninterScore = -1, float interScore = -1) override;
	virtual bool hasNode(const GridState<N>& node) override;
	virtual std::pair<float, float> getNodeScores(const GridState<N>& node) override;
	virtual void pushToEdgeQueue(const GridState<N>& node, int depth) override;
	virtual std::pair<GridState<N>, int> popFromEdgeQueue() override;
	virtual bool edgeQueueEmpty() override;
	virtual void addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) override;
	virtual bool hasEdge(const GridState<N>& node) override;
	virtual std::vector<GridState<N>> getEdges(const GridState<N>& node) override;
	virtual float getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) override;
	virtual std::vector<GridState<N>> getParents(const GridState<N>& child) override;
	virtual void copyNodesToScoreQueue() override;
	virtual void pushToScoreQueue(const GridState<N>& node) override;
	virtual GridState<N> popFromScoreQueue() override;
	virtual bool scoreQueueEmpty() override;
	virtual void addInterScore(const GridState<N>&node, float score) override;
	virtual void addNonInterScore(const GridState<N>& node, float score) override;
private:
	ankerl::unordered_dense::map<GridState<N>, std::pair<float,float> /*final_score, intermediate_score*/> m_nodes;
	ankerl::unordered_dense::map<GridState<N>, ankerl::unordered_dense::map<GridState<N>, float>> m_edges;
	ankerl::unordered_dense::map<GridState<N>, std::vector<GridState<N>>> m_reverseEdges;
	ankerl::unordered_dense::set<std::pair<GridState<N>, bool>> m_edgesGenerated;
	std::deque<std::pair<GridState<N>, int>> m_edgeQueue;
	std::deque<GridState<N>> m_scoreQueue;
};

template<uint N>
void InMemoryTablebase<N>::init(uint64 maxActions, int maxDepth) {
	ITablebase<N>::init(maxActions, maxDepth);
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
void InMemoryTablebase<N>::setNode(const GridState<N>& node, float noninterScore, float interScore) {
	m_nodes[node] = std::make_pair(noninterScore, interScore);
}

template<uint N>
bool InMemoryTablebase<N>::hasNode(const GridState<N>& node) {
	return m_nodes.count(node) == 1;
}

template<uint N>
std::pair<float, float> InMemoryTablebase<N>::getNodeScores(const GridState<N>& node) {
	return m_nodes[node];
}

template<uint N>
void InMemoryTablebase<N>::pushToEdgeQueue(const GridState<N>& node, int depth) {
	m_edgeQueue.push_back(std::make_pair(node, depth));
}

template<uint N>
std::pair<GridState<N>, int> InMemoryTablebase<N>::popFromEdgeQueue() {
	std::pair<GridState<N>, int> firstElement = m_edgeQueue.front();
	m_edgeQueue.pop_front();
	return firstElement;
}

template<uint N>
bool InMemoryTablebase<N>::edgeQueueEmpty() {
	return m_edgeQueue.empty();
}

template<uint N>
void InMemoryTablebase<N>::addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) {
	m_edges[parent][child] = weight;
	m_reverseEdges[child].push_back(parent);
}

template<uint N>
bool InMemoryTablebase<N>::hasEdge(const GridState<N>& node) {
	return m_edges.count(node) == 1;
}

template<uint N>
std::vector<GridState<N>> InMemoryTablebase<N>::getEdges(const GridState<N>& node) {
	std::vector<GridState<N>> edges;
	for (const auto& p : m_edges[node]) {
		edges.push_back(p.first);
	}
	return edges;
}

template<uint N>
float InMemoryTablebase<N>::getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) {
	return m_edges[parent][child];
}

template<uint N>
std::vector<GridState<N>> InMemoryTablebase<N>::getParents(const GridState<N>& child) {
	std::vector<GridState<N>> parents;
	auto it = m_reverseEdges.find(child);
	if (it != m_reverseEdges.end()) {
		for (const GridState<N>& prevState : it->second) {
			parents.push_back(prevState);
		}
	}
	return parents;
}

template<uint N>
void InMemoryTablebase<N>::copyNodesToScoreQueue() {
	for (const auto& p : m_nodes) {
		const GridState<N>& state = p.first;
		m_scoreQueue.push_back(state);
	}
}

template<uint N>
void InMemoryTablebase<N>::pushToScoreQueue(const GridState<N>& node) {
	m_scoreQueue.push_back(node);
}

template<uint N>
GridState<N> InMemoryTablebase<N>::popFromScoreQueue() {
	GridState<N> firstElement = m_scoreQueue.front();
	m_scoreQueue.pop_front();
	return firstElement;
}

template<uint N>
bool InMemoryTablebase<N>::scoreQueueEmpty() {
	return m_scoreQueue.empty();
}

template<uint N>
void InMemoryTablebase<N>::addInterScore(const GridState<N>& node, float score) {
	m_nodes[node].second = score;
}

template<uint N>
void InMemoryTablebase<N>::addNonInterScore(const GridState<N>& node, float score) {
	m_nodes[node].first = score;
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
class SqliteTablebase : public ITablebase<N> {
public:
	SqliteTablebase(float fourChance, const std::string& dbName = "", int cacheSize = -8388608 /*8GiB*/);
	virtual ~SqliteTablebase();
	//virtual void init(uint64 maxActions = UINT64_MAX, int maxDepth = -1) {}
	virtual float query(const GridState<N>& state) const { return 0; }
	virtual void recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const {}
	virtual std::pair<int, int> bestMove(const GridState<N>& state) const { return { -1,-1 }; }
protected:
	//TODO implement
	virtual void setNode(const GridState<N>& node, float noninterScore = -1, float interScore = -1) override {}
	virtual bool hasNode(const GridState<N>& node) { return false; }
	virtual std::pair<float, float> getNodeScores(const GridState<N>& node) override { return std::make_pair(-1.0f, -1.0f); }
	virtual void pushToEdgeQueue(const GridState<N>& node, int depth) override {}
	virtual std::pair<GridState<N>, int> popFromEdgeQueue() { return std::make_pair(GridState<N>(), 0); }
	virtual bool edgeQueueEmpty() override { return false; }
	virtual void addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) {}
	virtual bool hasEdge(const GridState<N>& node) { return false; }
	virtual std::vector<GridState<N>> getEdges(const GridState<N>& node) { return std::vector<GridState<N>>(); }
	virtual float getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) { return -1; }
	virtual std::vector<GridState<N>> getParents(const GridState<N>& child) { return std::vector<GridState<N>>(); }
	//virtual void generateEdges(uint64 maxActions, int maxDepth) override;
	//virtual void calculateScores(uint64 maxActions) override;
	virtual void copyNodesToScoreQueue() {}
	virtual void pushToScoreQueue(const GridState<N>& node) {}
	virtual GridState<N> popFromScoreQueue() { return GridState<N>(); }
	virtual bool scoreQueueEmpty() { return false; }
	virtual void addInterScore(const GridState<N>& node, float score) {}
	virtual void addNonInterScore(const GridState<N>& node, float score) {}
private:
	sqlite3* m_db;

	sqlite3_stmt* m_psInsertNode;
	sqlite3_stmt* m_psUpdateNodeInter;
	sqlite3_stmt* m_psUpdateNodeNoninter;
	sqlite3_stmt* m_psQueryNodeScores;
	sqlite3_stmt* m_psQueryNodeExists;

	sqlite3_stmt* m_psInsertEdge;
	sqlite3_stmt* m_psQueryEdges;
	sqlite3_stmt* m_psQueryReverseEdges;

	sqlite3_stmt* m_psEdgeQueueGetFront;
	sqlite3_stmt* m_psEdgeQueuePopFront;
	sqlite3_stmt* m_psEdgeQueuePushBack;

	sqlite3_stmt* m_psScoreQueueGetFront;
	sqlite3_stmt* m_psScoreQueuePopFront;
	sqlite3_stmt* m_psScoreQueuePushBack;

	static constexpr char PRAGMA_SYNCHRONOUS_SQL[] = "PRAGMA synchronous = OFF;";
	static constexpr char PRAGMA_JOURNAL_SQL[] = "PRAGMA journal_mode = MEMORY;";

	static constexpr char CREATE_TABLE_NODE_SQL[] = "CREATE TABLE node (\n"
		"grid_state BLOB PRIMARY KEY,\n"
		"inter_score REAL NOT NULL,\n"
		"noninter_score REAL NOT NULL) STRICT;";

	static constexpr char CREATE_TABLE_EDGE_SQL[] = "CREATE TABLE edge (\n"
		"start_state BLOB NOT NULL,\n"
		"end_state BLOB NOT NULL,\n"
		"weight REAL NOT NULL,\n"
		"PRIMARY KEY(start_state, end_state)\n"
		") STRICT;";

	static constexpr char CREATE_TABLE_EDGE_QUEUE_SQL[] = "CREATE TABLE edge_queue(\n"
		"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"node BLOB NOT NULL,\n"
		"node_depth INTEGER NOT NULL\n"
		") STRICT;";

	static constexpr char CREATE_TABLE_SCORE_QUEUE_SQL[] = "CREATE TABLE score_queue (\n"
		"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"node BLOB NOT NULL\n"
		") STRICT;";

	static constexpr char CREATE_TABLE_CONFIG_SQL[] = "CREATE TABLE config(\n"
		"prop_name TEXT PRIMARY KEY,\n"
		"prop_value TEXT\n"
		") STRICT;";

	static constexpr char CREATE_INDEX_REVERSE_EDGE_SQL[] = "CREATE INDEX reverse_edge ON edge(end_state);";

	static constexpr char INSERT_NODE_SQL[] = "INSERT INTO node(grid_state, inter_score, noninter_score) VALUES (?, -1.0, -1.0);";
	static constexpr char UPDATE_NODE_INTER_SQL[] = "UPDATE node SET inter_score = ? WHERE grid_state = ?;";
	static constexpr char UPDATE_NODE_NONINTER_SQL[] = "UPDATE node SET noninter_score = ? WHERE grid_state = ?;";
	static constexpr char QUERY_NODE_SCORES_SQL[] = "SELECT inter_score, noninter_score FROM node WHERE grid_state = ?;";
	static constexpr char QUERY_NODE_EXISTS_SQL[] = "SELECT count(*) FROM node WHERE grid_state = ?;";

	static constexpr char INSERT_EDGE_SQL[] = "INSERT INTO edge(start_state, end_state, weight) VALUES(?, ?, ?);";
	static constexpr char QUERY_EDGES_SQL[] = "SELECT end_state FROM edge WHERE start_state = ?;";
	static constexpr char QUERY_REVERSE_EDGES_SQL[] = "SELECT start_state FROM edge WHERE end_state = ?;";

	static constexpr char EDGE_QUEUE_GET_FRONT_SQL[] = "SELECT node, node_depth from edge_queue WHERE id = (SELECT min(id) FROM edge_queue);";
	static constexpr char EDGE_QUEUE_POP_FRONT_SQL[] = "DELETE from edge_queue WHERE id = (SELECT min(id) FROM edge_queue);";
	static constexpr char EDGE_QUEUE_PUSH_BACK_SQL[] = "INSERT INTO edge_queue(node, node_depth) VALUES(?,?);";

	static constexpr char SCORE_QUEUE_GET_FRONT_SQL[] = "SELECT node from score_queue WHERE id = (SELECT min(id) FROM score_queue);";
	static constexpr char SCORE_QUEUE_POP_FRONT_SQL[] = "DELETE from score_queue WHERE id = (SELECT min(id) FROM score_queue);";
	static constexpr char SCORE_QUEUE_PUSH_BACK_SQL[] = "INSERT INTO score_queue(node) VALUES(?);";
	static constexpr char COPY_NODES_TO_SCORE_QUEUE_SQL[] = "INSERT INTO score_queue(node) SELECT grid_state FROM node;";
};

template<uint N>
SqliteTablebase<N>::SqliteTablebase(float fourChance, const std::string& dbName, int cacheSize) : ITablebase<N>(fourChance) {
	if (dbName.empty()) {
		std::ostringstream dbNameStr;
		dbNameStr << "2048_tb_" << N << '-' << fourChance << ".sqlite";
		CHECK_RETURN_CODE(sqlite3_open(dbNameStr.str().c_str(), &m_db), SQLITE_OK);
	}
	else {
		CHECK_RETURN_CODE(sqlite3_open(dbName.c_str(), &m_db), SQLITE_OK);
	}

	CHECK_RETURN_CODE(sqlite3_exec(m_db, PRAGMA_SYNCHRONOUS_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, PRAGMA_JOURNAL_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	std::ostringstream cacheSizeStr;
	cacheSizeStr << "PRAGMA cache_size = " << cacheSize << ";";
	CHECK_RETURN_CODE(sqlite3_exec(m_db, cacheSizeStr.str().c_str(), nullptr, nullptr, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_exec(m_db, CREATE_TABLE_NODE_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, CREATE_TABLE_EDGE_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, CREATE_TABLE_EDGE_QUEUE_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, CREATE_TABLE_SCORE_QUEUE_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, CREATE_TABLE_CONFIG_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, CREATE_INDEX_REVERSE_EDGE_SQL, nullptr, nullptr, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, INSERT_NODE_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psInsertNode, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, UPDATE_NODE_INTER_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psUpdateNodeInter, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, UPDATE_NODE_NONINTER_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psUpdateNodeNoninter, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_NODE_SCORES_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryNodeScores, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_NODE_EXISTS_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryNodeExists, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, INSERT_EDGE_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psInsertEdge, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_EDGES_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryEdges, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_REVERSE_EDGES_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryReverseEdges, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, EDGE_QUEUE_GET_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psEdgeQueueGetFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, EDGE_QUEUE_POP_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psEdgeQueuePopFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, EDGE_QUEUE_PUSH_BACK_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psEdgeQueuePushBack, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, SCORE_QUEUE_GET_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psScoreQueueGetFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, SCORE_QUEUE_POP_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psScoreQueuePopFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, SCORE_QUEUE_PUSH_BACK_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psScoreQueuePushBack, nullptr), SQLITE_OK);
}

template<uint N>
SqliteTablebase<N>::~SqliteTablebase() {
	sqlite3_finalize(m_psInsertNode);
	sqlite3_finalize(m_psUpdateNodeInter);
	sqlite3_finalize(m_psUpdateNodeNoninter);
	sqlite3_finalize(m_psQueryNodeScores);
	sqlite3_finalize(m_psQueryNodeExists);

	sqlite3_finalize(m_psInsertEdge);
	sqlite3_finalize(m_psQueryEdges);
	sqlite3_finalize(m_psQueryReverseEdges);

	sqlite3_finalize(m_psEdgeQueueGetFront);
	sqlite3_finalize(m_psEdgeQueuePopFront);
	sqlite3_finalize(m_psEdgeQueuePushBack);

	sqlite3_finalize(m_psScoreQueueGetFront);
	sqlite3_finalize(m_psScoreQueuePopFront);
	sqlite3_finalize(m_psScoreQueuePushBack);

	sqlite3_close(m_db);
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

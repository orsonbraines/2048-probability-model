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
	ITablebase(float fourChance) :
		m_fourChance(fourChance),
		m_edgeQueueInitialized{ false },
		m_scoreQueueInitialized{ false },
		m_actionCount{ 0 },
		m_totalActions{ 0 } {}
	virtual ~ITablebase() {}
	virtual void init(uint64 maxActions = UINT64_MAX, int maxDepth = -1);
	virtual bool partialInit(uint64 maxActions, int maxDepth = -1);
	virtual float query(const GridState<N>& state) const = 0;
	virtual void recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const;
	virtual std::pair<int, int> bestMove(const GridState<N>& state) const;
protected:
	virtual void initializeEdgeQueue();
	virtual void generateEdges(uint64 maxActions, int maxDepth);
	virtual void calculateScores(uint64 maxActions);
	virtual void setNode(const GridState<N>& node, float noninterScore = -1, float interScore = -1) = 0;
	virtual bool hasNode(const GridState<N>& node) const = 0;
	virtual std::pair<float, float> getNodeScores(const GridState<N>& node) const = 0;
	virtual void pushToEdgeQueue(const GridState<N>& node, int depth) = 0;
	virtual std::pair<GridState<N>, int> popFromEdgeQueue() = 0;
	virtual bool edgeQueueEmpty() = 0;
	virtual void addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) = 0;
	virtual bool hasEdge(const GridState<N>& node) const = 0;
	virtual std::vector<GridState<N>> getEdges(const GridState<N>& node) const = 0;
	virtual float getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) const = 0;
	virtual std::vector<GridState<N>> getParents(const GridState<N>& child) const = 0;
	virtual void copyNodesToScoreQueue() = 0;
	virtual void pushToScoreQueue(const GridState<N>& node) = 0;
	virtual GridState<N> popFromScoreQueue() = 0;
	virtual bool scoreQueueEmpty() const = 0;
	virtual void addInterScore(const GridState<N>& node, float score) = 0;
	virtual void addNonInterScore(const GridState<N>& node, float score) = 0;
	const float m_fourChance;
	bool m_edgeQueueInitialized;
	bool m_scoreQueueInitialized;
	uint64 m_actionCount;
	uint64 m_totalActions;
};

template<uint N>
void ITablebase<N>::initializeEdgeQueue() {
	m_edgeQueueInitialized = true;
	setNode(GridState<N>());
	pushToEdgeQueue(GridState<N>(), 0);
}

template<uint N>
void ITablebase<N>::init(uint64 maxActions, int maxDepth) {
	initializeEdgeQueue();
	generateEdges(maxActions, maxDepth);
	copyNodesToScoreQueue();
	m_scoreQueueInitialized = true;
	calculateScores(maxActions);
}

template<uint N>
bool ITablebase<N>::partialInit(uint64 maxActions, int maxDepth) {
	if (!m_edgeQueueInitialized) {
		initializeEdgeQueue();
	}
	if (!edgeQueueEmpty()) {
		generateEdges(maxActions, maxDepth);
		return false;
	}
	if (!m_scoreQueueInitialized) {
		copyNodesToScoreQueue();
		m_scoreQueueInitialized = true;
	}
	if (!scoreQueueEmpty()) {
		calculateScores(maxActions);
	}
	return scoreQueueEmpty();
}

template<uint N>
void ITablebase<N>::generateEdges(uint64 maxActions, int maxDepth) {
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

	m_totalActions += m_actionCount;
	DEBUG_LOG("generateEdges exiting after " << m_actionCount << " actions (" << m_totalActions << " total)" << std::endl);
	m_actionCount = 0;
}

template<uint N>
void ITablebase<N>::calculateScores(uint64 maxActions) {
	for (; m_actionCount < maxActions && !scoreQueueEmpty(); ++m_actionCount) {
		GridState<N> state = popFromScoreQueue();
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
	m_totalActions += m_actionCount;
	DEBUG_LOG("calculateScores exiting after " << m_actionCount << " actions (" << m_totalActions << " total)" << std::endl);
	m_actionCount = 0;
}

template<uint N>
std::pair<int, int> ITablebase<N>::bestMove(const GridState<N>& state) const {
	float bestScore = 0;
	GridState<N> bestState;
	if (hasEdge(state)) {
		std::vector<GridState<N>> children = getEdges(state);
		// find best intermediate child score
		for (const GridState<N>& child : children) {
			if (getEdgeWeight(state, child) != -1) continue;
			float score = getNodeScores(child).second;
			if (score > bestScore) {
				bestScore = score;
				bestState = child;
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
void ITablebase<N>::recursiveQuery(const GridState<N>& state, int currDepth, int maxDepth, QueryResultsType<N>& results) const {
	if (currDepth > maxDepth) return;
	bool intermediate = currDepth & 1;
	auto scores = getNodeScores(state);
	results.emplace_back(std::make_tuple(currDepth, state, intermediate ? scores.second : scores.first));
	if (hasEdge(state)) {
		auto children = getEdges(state);
		for (const auto& child : children) {
			float edgeWeight = getEdgeWeight(state, child);
			if ((!intermediate && edgeWeight != -1.0f) || (intermediate && edgeWeight == -1.0f)) continue;
			recursiveQuery(child, currDepth + 1, maxDepth, results);
		}
	}
}

template<uint N>
class InMemoryTablebase : public ITablebase<N> {
public:
	InMemoryTablebase(float fourChance) : ITablebase<N>(fourChance) {}
	virtual void init(uint64 maxActions = UINT64_MAX, int maxDepth = -1) override;
	virtual float query(const GridState<N>& state) const override;
	void dump(std::ostream &o) const;
protected:
	virtual void setNode(const GridState<N>& node, float noninterScore = -1, float interScore = -1) override;
	virtual bool hasNode(const GridState<N>& node) const override;
	virtual std::pair<float, float> getNodeScores(const GridState<N>& node) const override;
	virtual void pushToEdgeQueue(const GridState<N>& node, int depth) override;
	virtual std::pair<GridState<N>, int> popFromEdgeQueue() override;
	virtual bool edgeQueueEmpty() override;
	virtual void addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) override;
	virtual bool hasEdge(const GridState<N>& node) const override;
	virtual std::vector<GridState<N>> getEdges(const GridState<N>& node) const override;
	virtual float getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) const override;
	virtual std::vector<GridState<N>> getParents(const GridState<N>& child) const override;
	virtual void copyNodesToScoreQueue() override;
	virtual void pushToScoreQueue(const GridState<N>& node) override;
	virtual GridState<N> popFromScoreQueue() override;
	virtual bool scoreQueueEmpty() const override;
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
void InMemoryTablebase<N>::setNode(const GridState<N>& node, float noninterScore, float interScore) {
	m_nodes[node] = std::make_pair(noninterScore, interScore);
}

template<uint N>
bool InMemoryTablebase<N>::hasNode(const GridState<N>& node) const {
	return m_nodes.count(node) == 1;
}

template<uint N>
std::pair<float, float> InMemoryTablebase<N>::getNodeScores(const GridState<N>& node) const {
	return m_nodes.at(node);
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
bool InMemoryTablebase<N>::hasEdge(const GridState<N>& node) const {
	return m_edges.count(node) == 1;
}

template<uint N>
std::vector<GridState<N>> InMemoryTablebase<N>::getEdges(const GridState<N>& node) const {
	std::vector<GridState<N>> edges;
	for (const auto& p : m_edges.at(node)) {
		edges.push_back(p.first);
	}
	return edges;
}

template<uint N>
float InMemoryTablebase<N>::getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) const {
	return m_edges.at(parent).at(child);
}

template<uint N>
std::vector<GridState<N>> InMemoryTablebase<N>::getParents(const GridState<N>& child) const {
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
	for (const auto &p : m_nodes) {
		const GridState<N>& state = p.first;
		m_scoreQueue.push_front(state);
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
bool InMemoryTablebase<N>::scoreQueueEmpty() const {
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
class SqliteTablebase : public ITablebase<N> {
public:
	SqliteTablebase(float fourChance, const std::string& dbName = "", int cacheSize = -16777216 /*16GiB*/);
	virtual ~SqliteTablebase();
	virtual float query(const GridState<N>& state) const override;
	virtual void generateEdges(uint64 maxActions, int maxDepth) override { beginTransaction(); ITablebase<N>::generateEdges(maxActions, maxDepth); commitTransaction(); }
	virtual void calculateScores(uint64 maxActions) override { beginTransaction(); ITablebase<N>::calculateScores(maxActions); commitTransaction(); }
protected:
	virtual void initializeEdgeQueue() override;
	virtual void setNode(const GridState<N>& node, float noninterScore = -1, float interScore = -1) override;
	virtual bool hasNode(const GridState<N>& node) const override;
	virtual std::pair<float, float> getNodeScores(const GridState<N>& node) const override;
	virtual void pushToEdgeQueue(const GridState<N>& node, int depth) override;
	virtual std::pair<GridState<N>, int> popFromEdgeQueue() override;
	virtual bool edgeQueueEmpty() override;
	virtual void addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) override;
	virtual bool hasEdge(const GridState<N>& node) const override;
	virtual std::vector<GridState<N>> getEdges(const GridState<N>& node) const override;
	virtual float getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) const override;
	virtual std::vector<GridState<N>> getParents(const GridState<N>& child) const override;
	virtual void copyNodesToScoreQueue() override;
	virtual void pushToScoreQueue(const GridState<N>& node) override;
	virtual GridState<N> popFromScoreQueue() override;
	virtual bool scoreQueueEmpty() const override;
	virtual void addInterScore(const GridState<N>& node, float score) override;
	virtual void addNonInterScore(const GridState<N>& node, float score) override;
private:
	void beginTransaction() {
		CHECK_RETURN_CODE(sqlite3_step(m_psBegin), SQLITE_DONE);
		CHECK_RETURN_CODE(sqlite3_reset(m_psBegin), SQLITE_OK);
	}

	void commitTransaction() {
		CHECK_RETURN_CODE(sqlite3_step(m_psCommit), SQLITE_DONE);
		CHECK_RETURN_CODE(sqlite3_reset(m_psCommit), SQLITE_OK);
	}

	sqlite3* m_db;

	sqlite3_stmt* m_psBegin;
	sqlite3_stmt* m_psCommit;

	sqlite3_stmt* m_psInsertNode;
	sqlite3_stmt* m_psUpdateNodeInter;
	sqlite3_stmt* m_psUpdateNodeNoninter;
	sqlite3_stmt* m_psQueryNodeScores;
	sqlite3_stmt* m_psQueryNodeExists;

	sqlite3_stmt* m_psInsertEdge;
	sqlite3_stmt* m_psQueryEdge;
	sqlite3_stmt* m_psQueryEdges;
	sqlite3_stmt* m_psQueryReverseEdges;

	sqlite3_stmt* m_psEdgeQueueGetFront;
	sqlite3_stmt* m_psEdgeQueuePopFront;
	sqlite3_stmt* m_psEdgeQueuePushBack;

	sqlite3_stmt* m_psScoreQueueGetFront;
	sqlite3_stmt* m_psScoreQueuePopFront;
	sqlite3_stmt* m_psScoreQueuePushBack;
	sqlite3_stmt* m_psCopyNodesToScoreQueue;

	static constexpr char PRAGMA_SYNCHRONOUS_SQL[] = "PRAGMA synchronous = OFF;";
	static constexpr char PRAGMA_JOURNAL_SQL[] = "PRAGMA journal_mode = MEMORY;";
	static constexpr char BEGIN_SQL[] = "BEGIN;";
	static constexpr char COMMIT_SQL[] = "COMMIT;";

	static constexpr char CREATE_TABLE_NODE_SQL[] = "CREATE TABLE IF NOT EXISTS node (\n"
		"grid_state BLOB PRIMARY KEY,\n"
		"inter_score REAL NOT NULL,\n"
		"noninter_score REAL NOT NULL) STRICT;";

	static constexpr char CREATE_TABLE_EDGE_SQL[] = "CREATE TABLE IF NOT EXISTS edge (\n"
		"start_state BLOB NOT NULL,\n"
		"end_state BLOB NOT NULL,\n"
		"weight REAL NOT NULL,\n"
		"PRIMARY KEY(start_state, end_state)\n"
		") STRICT;";

	static constexpr char CREATE_TABLE_EDGE_QUEUE_SQL[] = "CREATE TABLE IF NOT EXISTS edge_queue(\n"
		"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"node BLOB NOT NULL,\n"
		"node_depth INTEGER NOT NULL\n"
		") STRICT;";

	static constexpr char CREATE_TABLE_SCORE_QUEUE_SQL[] = "CREATE TABLE IF NOT EXISTS score_queue (\n"
		"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"node BLOB NOT NULL\n"
		") STRICT;";

	static constexpr char CREATE_TABLE_CONFIG_SQL[] = "CREATE TABLE IF NOT EXISTS config(\n"
		"prop_name TEXT PRIMARY KEY,\n"
		"prop_value TEXT\n"
		") STRICT;";

	static constexpr char CREATE_INDEX_REVERSE_EDGE_SQL[] = "CREATE INDEX IF NOT EXISTS reverse_edge ON edge(end_state);";

	static constexpr char INSERT_NODE_SQL[] = "INSERT INTO node(grid_state, inter_score, noninter_score) VALUES (?, ?, ?);";
	static constexpr char UPDATE_NODE_INTER_SQL[] = "UPDATE node SET inter_score = ? WHERE grid_state = ?;";
	static constexpr char UPDATE_NODE_NONINTER_SQL[] = "UPDATE node SET noninter_score = ? WHERE grid_state = ?;";
	static constexpr char QUERY_NODE_SCORES_SQL[] = "SELECT inter_score, noninter_score FROM node WHERE grid_state = ?;";
	static constexpr char QUERY_NODE_EXISTS_SQL[] = "SELECT count(*) FROM node WHERE grid_state = ?;";

	static constexpr char INSERT_EDGE_SQL[] = "INSERT INTO edge(start_state, end_state, weight) VALUES(?, ?, ?);";
	static constexpr char QUERY_EDGE_SQL[] = "SELECT weight FROM edge WHERE start_state = ? AND end_state = ?;";
	static constexpr char QUERY_EDGES_SQL[] = "SELECT end_state FROM edge WHERE start_state = ?;";
	static constexpr char QUERY_REVERSE_EDGES_SQL[] = "SELECT start_state FROM edge WHERE end_state = ?;";

	static constexpr char EDGE_QUEUE_INIT_SQL[] = "INSERT INTO config(prop_name, prop_value) VALUES ('edge_queue_init','TRUE');";
	static constexpr char EDGE_QUEUE_IS_INIT_SQL[] = "SELECT 1 FROM config WHERE prop_name = 'edge_queue_init';";
	static constexpr char EDGE_QUEUE_GET_FRONT_SQL[] = "SELECT node, node_depth from edge_queue WHERE id = (SELECT min(id) FROM edge_queue);";
	static constexpr char EDGE_QUEUE_POP_FRONT_SQL[] = "DELETE from edge_queue WHERE id = (SELECT min(id) FROM edge_queue);";
	static constexpr char EDGE_QUEUE_PUSH_BACK_SQL[] = "INSERT INTO edge_queue(node, node_depth) VALUES(?,?);";

	static constexpr char SCORE_QUEUE_INIT_SQL[] = "INSERT INTO config(prop_name, prop_value) VALUES ('score_queue_init','TRUE');";
	static constexpr char SCORE_QUEUE_IS_INIT_SQL[] = "SELECT 1 FROM config WHERE prop_name = 'score_queue_init';";
	static constexpr char SCORE_QUEUE_GET_FRONT_SQL[] = "SELECT node from score_queue WHERE id = (SELECT min(id) FROM score_queue);";
	static constexpr char SCORE_QUEUE_POP_FRONT_SQL[] = "DELETE from score_queue WHERE id = (SELECT min(id) FROM score_queue);";
	static constexpr char SCORE_QUEUE_PUSH_BACK_SQL[] = "INSERT INTO score_queue(node) VALUES(?);";
	static constexpr char COPY_NODES_TO_SCORE_QUEUE_SQL[] = "INSERT INTO score_queue(node) SELECT grid_state FROM node ORDER BY rowid DESC;";
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

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, BEGIN_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psBegin, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, COMMIT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psCommit, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, INSERT_NODE_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psInsertNode, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, UPDATE_NODE_INTER_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psUpdateNodeInter, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, UPDATE_NODE_NONINTER_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psUpdateNodeNoninter, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_NODE_SCORES_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryNodeScores, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_NODE_EXISTS_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryNodeExists, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, INSERT_EDGE_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psInsertEdge, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_EDGE_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryEdge, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_EDGES_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryEdges, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, QUERY_REVERSE_EDGES_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psQueryReverseEdges, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, EDGE_QUEUE_GET_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psEdgeQueueGetFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, EDGE_QUEUE_POP_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psEdgeQueuePopFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, EDGE_QUEUE_PUSH_BACK_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psEdgeQueuePushBack, nullptr), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, SCORE_QUEUE_GET_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psScoreQueueGetFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, SCORE_QUEUE_POP_FRONT_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psScoreQueuePopFront, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, SCORE_QUEUE_PUSH_BACK_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psScoreQueuePushBack, nullptr), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_prepare_v3(m_db, COPY_NODES_TO_SCORE_QUEUE_SQL, -1, SQLITE_PREPARE_PERSISTENT, &m_psCopyNodesToScoreQueue, nullptr), SQLITE_OK);

	// Set the values of m_edgeQueueInitialize and m_scoreQueueInitialized from the config table;
	sqlite3_stmt* psQueueIsInit;
	CHECK_RETURN_CODE(sqlite3_prepare_v2(m_db, EDGE_QUEUE_IS_INIT_SQL, -1, &psQueueIsInit, nullptr), SQLITE_OK);
	int returnCode = sqlite3_step(psQueueIsInit);
	if (returnCode == SQLITE_ROW) {
		ITablebase<N>::m_edgeQueueInitialized = true;
	}
	else {
		CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
		ITablebase<N>::m_edgeQueueInitialized = false;
	}
	CHECK_RETURN_CODE(sqlite3_reset(psQueueIsInit), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_finalize(psQueueIsInit), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_prepare_v2(m_db, SCORE_QUEUE_IS_INIT_SQL, -1, &psQueueIsInit, nullptr), SQLITE_OK);
	returnCode = sqlite3_step(psQueueIsInit);
	if (returnCode == SQLITE_ROW) {
		ITablebase<N>::m_scoreQueueInitialized = true;
	}
	else {
		CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
		ITablebase<N>::m_scoreQueueInitialized = false;
	}
	CHECK_RETURN_CODE(sqlite3_reset(psQueueIsInit), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_finalize(psQueueIsInit), SQLITE_OK);
}

template<uint N>
SqliteTablebase<N>::~SqliteTablebase() {
	sqlite3_finalize(m_psBegin);
	sqlite3_finalize(m_psCommit);

	sqlite3_finalize(m_psInsertNode);
	sqlite3_finalize(m_psUpdateNodeInter);
	sqlite3_finalize(m_psUpdateNodeNoninter);
	sqlite3_finalize(m_psQueryNodeScores);
	sqlite3_finalize(m_psQueryNodeExists);

	sqlite3_finalize(m_psInsertEdge);
	sqlite3_finalize(m_psQueryEdge);
	sqlite3_finalize(m_psQueryEdges);
	sqlite3_finalize(m_psQueryReverseEdges);

	sqlite3_finalize(m_psEdgeQueueGetFront);
	sqlite3_finalize(m_psEdgeQueuePopFront);
	sqlite3_finalize(m_psEdgeQueuePushBack);

	sqlite3_finalize(m_psScoreQueueGetFront);
	sqlite3_finalize(m_psScoreQueuePopFront);
	sqlite3_finalize(m_psScoreQueuePushBack);
	sqlite3_finalize(m_psCopyNodesToScoreQueue);

	sqlite3_close(m_db);
}

template<uint N>
void SqliteTablebase<N>::initializeEdgeQueue() {
	ITablebase<N>::initializeEdgeQueue();
	CHECK_RETURN_CODE(sqlite3_exec(m_db, EDGE_QUEUE_INIT_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
}

template<uint N>
float SqliteTablebase<N>::query(const GridState<N>& state) const {
	if (hasNode(state)) {
		return getNodeScores(state).first;
	}
	else {
		return -1;
	}
}

template<uint N>
void SqliteTablebase<N>::setNode(const GridState<N>& node, float noninterScore, float interScore) {
	if (hasNode(node)) {
		addInterScore(node, interScore);
		addNonInterScore(node, noninterScore);
	}
	else {
		CHECK_RETURN_CODE(sqlite3_bind_blob(m_psInsertNode, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
		CHECK_RETURN_CODE(sqlite3_bind_double(m_psInsertNode, 2, interScore), SQLITE_OK);
		CHECK_RETURN_CODE(sqlite3_bind_double(m_psInsertNode, 3, noninterScore), SQLITE_OK);
		CHECK_RETURN_CODE(sqlite3_step(m_psInsertNode), SQLITE_DONE);
		CHECK_RETURN_CODE(sqlite3_reset(m_psInsertNode), SQLITE_OK);
	}
}

template<uint N>
bool SqliteTablebase<N>::hasNode(const GridState<N>& node) const {
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryNodeExists, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psQueryNodeExists), SQLITE_ROW);
	bool foundNode = (sqlite3_column_int(m_psQueryNodeExists, 0) == 1);
	CHECK_RETURN_CODE(sqlite3_step(m_psQueryNodeExists), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psQueryNodeExists), SQLITE_OK);
	return foundNode;
}

template<uint N>
std::pair<float, float> SqliteTablebase<N>::getNodeScores(const GridState<N>& node) const {
	QUERY_NODE_SCORES_SQL;
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryNodeScores, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psQueryNodeScores), SQLITE_ROW);
	double interScore = sqlite3_column_double(m_psQueryNodeScores, 0);
	double noninterScore = sqlite3_column_double(m_psQueryNodeScores, 1);
	CHECK_RETURN_CODE(sqlite3_step(m_psQueryNodeScores), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psQueryNodeScores), SQLITE_OK);
	return std::make_pair(float(noninterScore), float(interScore));
}

template<uint N>
void SqliteTablebase<N>::pushToEdgeQueue(const GridState<N>& node, int depth) {
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psEdgeQueuePushBack, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_bind_int(m_psEdgeQueuePushBack, 2, depth), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psEdgeQueuePushBack), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psEdgeQueuePushBack), SQLITE_OK);
}

template<uint N>
std::pair<GridState<N>, int> SqliteTablebase<N>::popFromEdgeQueue() {
	CHECK_RETURN_CODE(sqlite3_step(m_psEdgeQueueGetFront), SQLITE_ROW);
	GridState<N> node;
	const void* nodeGrid = sqlite3_column_blob(m_psEdgeQueueGetFront, 0);
	std::memcpy(node.gridData(), nodeGrid, sizeof(node.getGrid()));
	int nodeDepth = sqlite3_column_int(m_psEdgeQueueGetFront, 1);
	CHECK_RETURN_CODE(sqlite3_step(m_psEdgeQueueGetFront), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psEdgeQueueGetFront), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_step(m_psEdgeQueuePopFront), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psEdgeQueuePopFront), SQLITE_OK);

	return std::make_pair(node, nodeDepth);
}

template<uint N>
bool SqliteTablebase<N>::edgeQueueEmpty() {
	int returnCode = sqlite3_step(m_psEdgeQueueGetFront);
	bool isEmpty;
	if (returnCode == SQLITE_ROW) {
		isEmpty = false;
	}
	else {
		CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
		isEmpty = true;
	}
	CHECK_RETURN_CODE(sqlite3_reset(m_psEdgeQueueGetFront), SQLITE_OK);
	return isEmpty;
}

template<uint N>
void SqliteTablebase<N>::addEdge(const GridState<N>& parent, const GridState<N>& child, float weight) {
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psInsertEdge, 1, parent.gridData(), sizeof(parent.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psInsertEdge, 2, child.gridData(), sizeof(child.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_bind_double(m_psInsertEdge, 3, weight), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psInsertEdge), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psInsertEdge), SQLITE_OK);
}

template<uint N>
bool SqliteTablebase<N>::hasEdge(const GridState<N>& node) const {
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryEdges, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	int returnCode = sqlite3_step(m_psQueryEdges);
	bool foundEdge;
	if (returnCode == SQLITE_ROW) {
		foundEdge = true;
	}
	else {
		CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
		foundEdge = false;
	}
	CHECK_RETURN_CODE(sqlite3_reset(m_psQueryEdges), SQLITE_OK);
	return foundEdge;
}

template<uint N>
std::vector<GridState<N>> SqliteTablebase<N>::getEdges(const GridState<N>& node) const {
	std::vector<GridState<N>> edges;
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryEdges, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	int returnCode;
	while ((returnCode = sqlite3_step(m_psQueryEdges)) == SQLITE_ROW) {
		edges.emplace_back();
		const void* childGrid = sqlite3_column_blob(m_psQueryEdges, 0);
		std::memcpy(edges.back().gridData(), childGrid, sizeof(node.getGrid()));
	}
	CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psQueryEdges), SQLITE_OK);
	return edges;
}

template<uint N>
float SqliteTablebase<N>::getEdgeWeight(const GridState<N>& parent, const GridState<N>& child) const {
	QUERY_EDGE_SQL;
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryEdge, 1, parent.gridData(), sizeof(parent.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryEdge, 2, child.gridData(), sizeof(child.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psQueryEdge), SQLITE_ROW);
	double weight = sqlite3_column_double(m_psQueryEdge, 0);
	CHECK_RETURN_CODE(sqlite3_step(m_psQueryEdge), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psQueryEdge), SQLITE_OK);
	return weight;
}

template<uint N>
std::vector<GridState<N>> SqliteTablebase<N>::getParents(const GridState<N>& child) const { 
	auto parents = std::vector<GridState<N>>();
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psQueryReverseEdges, 1, child.gridData(), sizeof(child.getGrid()), SQLITE_STATIC), SQLITE_OK);
	int returnCode;
	while ((returnCode = sqlite3_step(m_psQueryReverseEdges)) == SQLITE_ROW) {
		parents.emplace_back();
		const void* parentGrid = sqlite3_column_blob(m_psQueryReverseEdges, 0);
		std::memcpy(parents.back().gridData(), parentGrid, sizeof(child.getGrid()));
	}
	CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psQueryReverseEdges), SQLITE_OK);
	return parents;
}

template<uint N>
void SqliteTablebase<N>::copyNodesToScoreQueue() {
	CHECK_RETURN_CODE(sqlite3_step(m_psCopyNodesToScoreQueue), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psCopyNodesToScoreQueue), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_exec(m_db, SCORE_QUEUE_INIT_SQL, nullptr, nullptr, nullptr), SQLITE_OK);
}

template<uint N>
void SqliteTablebase<N>::pushToScoreQueue(const GridState<N>& node) {
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psScoreQueuePushBack, 1, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psScoreQueuePushBack), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psScoreQueuePushBack), SQLITE_OK);
}

template<uint N>
GridState<N> SqliteTablebase<N>::popFromScoreQueue() {
	CHECK_RETURN_CODE(sqlite3_step(m_psScoreQueueGetFront), SQLITE_ROW);
	GridState<N> node;
	const void* nodeGrid = sqlite3_column_blob(m_psScoreQueueGetFront, 0);
	std::memcpy(node.gridData(), nodeGrid, sizeof(node.getGrid()));
	CHECK_RETURN_CODE(sqlite3_step(m_psScoreQueueGetFront), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psScoreQueueGetFront), SQLITE_OK);

	CHECK_RETURN_CODE(sqlite3_step(m_psScoreQueuePopFront), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psScoreQueuePopFront), SQLITE_OK);

	return node;
}

template<uint N>
bool SqliteTablebase<N>::scoreQueueEmpty() const {
	int returnCode = sqlite3_step(m_psScoreQueueGetFront);
	bool isEmpty;
	if (returnCode == SQLITE_ROW) {
		isEmpty = false;
	}
	else {
		CHECK_RETURN_CODE(returnCode, SQLITE_DONE);
		isEmpty = true;
	}
	CHECK_RETURN_CODE(sqlite3_reset(m_psScoreQueueGetFront), SQLITE_OK);
	return isEmpty;
}

template<uint N>
void SqliteTablebase<N>::addInterScore(const GridState<N>& node, float score) {
	CHECK_RETURN_CODE(sqlite3_bind_double(m_psUpdateNodeInter, 1, double(score)), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psUpdateNodeInter, 2, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psUpdateNodeInter), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psUpdateNodeInter), SQLITE_OK);
}

template<uint N>
void SqliteTablebase<N>::addNonInterScore(const GridState<N>& node, float score) {
	UPDATE_NODE_NONINTER_SQL;
	UPDATE_NODE_INTER_SQL;
	CHECK_RETURN_CODE(sqlite3_bind_double(m_psUpdateNodeNoninter, 1, double(score)), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_bind_blob(m_psUpdateNodeNoninter, 2, node.gridData(), sizeof(node.getGrid()), SQLITE_STATIC), SQLITE_OK);
	CHECK_RETURN_CODE(sqlite3_step(m_psUpdateNodeNoninter), SQLITE_DONE);
	CHECK_RETURN_CODE(sqlite3_reset(m_psUpdateNodeNoninter), SQLITE_OK);
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

template<uint N>
bool operator==(const ITablebase<N>& a, const ITablebase<N>& b) {
	GridState<N> state;
	for (;;) {
		float tolerance = 0.0000001f;
		float aScore = a.query(state);
		float bScore = b.query(state);
		if (std::abs(aScore - bScore) > tolerance) {
			DEBUG_LOG("difference found at: " << state << std::endl);
			DEBUG_LOG("a: " << aScore << " b: " << bScore << std::endl);
			return false;
		}
		for (uint i = 0;; ++i) {
			if (i == N * N) return true;
			uint tile = state.readTile(i / N, i % N) + 1;
			if (tile == N * N + 2) {
				state.writeTile(i / N, i % N, 0);
			}
			else {
				state.writeTile(i / N, i % N, tile);
				break;
			}
		}
	}
}

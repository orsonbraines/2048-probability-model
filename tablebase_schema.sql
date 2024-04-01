CREATE TABLE node (
    grid_state BLOB PRIMARY KEY, 
    inter_score REAL NOT NULL, 
    noninter_score REAL NOT NULL) STRICT;

CREATE TABLE edge (
    start_state BLOB NOT NULL,
    end_state BLOB NOT NULL,
    weight REAL NOT NULL,
    PRIMARY KEY (start_state, end_state)
) STRICT;

CREATE TABLE edge_queue (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    node BLOB NOT NULL,
    node_depth INTEGER NOT NULL
) STRICT;

CREATE TABLE score_queue (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    node BLOB NOT NULL
) STRICT;

CREATE TABLE config (
    prop_name TEXT PRIMARY KEY,
    prop_value TEXT
) STRICT;

CREATE INDEX reverse_edge ON edge(end_state);
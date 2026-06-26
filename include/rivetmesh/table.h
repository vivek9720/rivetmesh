#pragma once

#include <map>
#include <string>
#include <vector>

#include "rivetmesh/status.h"

namespace rivetmesh {

enum class ColumnType {
  integer,
  decimal,
  text,
  flag,
};

struct Column {
  std::string name;
  ColumnType type = ColumnType::text;
};

struct Cell {
  std::string text;
  std::int64_t integer = 0;
  double decimal = 0.0;
  bool flag = false;
};

struct Row {
  std::map<std::string, Cell> cells;
};

struct Table {
  std::vector<Column> columns;
  std::vector<Row> rows;
  std::map<std::string, std::size_t> column_index;
};

Outcome<Table> parseTable(const std::string& text);
Outcome<void> validateTable(const Table& table);
std::string tableSummary(const Table& table);

}  // namespace rivetmesh

#include "rivetmesh/table.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace rivetmesh {
namespace {

std::string trim(std::string text) {
  auto space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(),
                                        [&](char ch) { return !space(ch); }));
  text.erase(std::find_if(text.rbegin(), text.rend(),
                          [&](char ch) { return !space(ch); })
                 .base(),
             text.end());
  return text;
}

std::vector<std::string> splitWords(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false;
  bool escape = false;
  for (char ch : line) {
    if (escape) {
      cur.push_back(ch);
      escape = false;
      continue;
    }
    if (ch == '\\') {
      escape = true;
      continue;
    }
    if (ch == '"') {
      quoted = !quoted;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch)) && !quoted) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
      continue;
    }
    cur.push_back(ch);
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

ColumnType parseType(const std::string& text) {
  if (text == "int" || text == "integer") {
    return ColumnType::integer;
  }
  if (text == "decimal" || text == "float") {
    return ColumnType::decimal;
  }
  if (text == "flag" || text == "bool") {
    return ColumnType::flag;
  }
  return ColumnType::text;
}

Outcome<Cell> parseCell(ColumnType type, const std::string& value, std::size_t line) {
  Cell cell;
  cell.text = value;
  try {
    switch (type) {
      case ColumnType::integer: {
        std::size_t consumed = 0;
        cell.integer = std::stoll(value, &consumed, 0);
        if (consumed != value.size()) {
          return Outcome<Cell>::err(fail(Errc::bad_number, "bad integer cell", line));
        }
        break;
      }
      case ColumnType::decimal: {
        char* end = nullptr;
        cell.decimal = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0') {
          return Outcome<Cell>::err(fail(Errc::bad_number, "bad decimal cell", line));
        }
        break;
      }
      case ColumnType::flag:
        cell.flag = value == "true" || value == "1" || value == "yes";
        break;
      case ColumnType::text:
        break;
    }
  } catch (...) {
    return Outcome<Cell>::err(fail(Errc::bad_number, "bad table cell", line));
  }
  return Outcome<Cell>::ok(std::move(cell));
}

Outcome<void> parseSchema(Table& table, const std::vector<std::string>& tokens,
                          std::size_t line) {
  if (tokens.size() < 2) {
    return Outcome<void>::err(fail(Errc::syntax, "schema expects columns", line));
  }
  for (std::size_t i = 1; i < tokens.size(); ++i) {
    const std::size_t colon = tokens[i].find(':');
    if (colon == std::string::npos || colon == 0) {
      return Outcome<void>::err(fail(Errc::syntax, "schema column expects name:type", line));
    }
    Column column;
    column.name = tokens[i].substr(0, colon);
    column.type = parseType(tokens[i].substr(colon + 1));
    table.column_index[column.name] = table.columns.size();
    table.columns.push_back(std::move(column));
  }
  return Outcome<void>::success();
}

Outcome<void> parseRow(Table& table, const std::vector<std::string>& tokens, std::size_t line) {
  if (table.columns.empty()) {
    return Outcome<void>::err(fail(Errc::state, "row before schema", line));
  }
  Row row;
  for (std::size_t i = 1; i < tokens.size(); ++i) {
    const std::size_t eq = tokens[i].find('=');
    if (eq == std::string::npos || eq == 0) {
      return Outcome<void>::err(fail(Errc::syntax, "row field expects key=value", line));
    }
    const std::string key = tokens[i].substr(0, eq);
    const std::string value = tokens[i].substr(eq + 1);
    auto found = table.column_index.find(key);
    if (found == table.column_index.end()) {
      return Outcome<void>::err(fail(Errc::syntax, "unknown row field", line));
    }
    auto cell = parseCell(table.columns[found->second].type, value, line);
    if (!cell) {
      return Outcome<void>::err(cell.status());
    }
    row.cells[key] = cell.take();
  }
  table.rows.push_back(std::move(row));
  return Outcome<void>::success();
}

}  // namespace

Outcome<Table> parseTable(const std::string& text) {
  std::istringstream input(text);
  std::string line;
  std::size_t line_number = 0;
  Table table;
  bool saw_header = false;

  while (std::getline(input, line)) {
    ++line_number;
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    auto tokens = splitWords(line);
    if (!saw_header) {
      if (tokens.size() != 1 || tokens[0] != "TBL2") {
        return Outcome<Table>::err(fail(Errc::bad_magic, "table missing TBL2", line_number));
      }
      saw_header = true;
      continue;
    }
    if (tokens.empty()) {
      continue;
    }
    Outcome<void> parsed = tokens[0] == "schema"
                               ? parseSchema(table, tokens, line_number)
                               : tokens[0] == "row" ? parseRow(table, tokens, line_number)
                                                     : Outcome<void>::err(fail(
                                                           Errc::syntax, "unknown table record",
                                                           line_number));
    if (!parsed) {
      return Outcome<Table>::err(parsed.status());
    }
    if (table.rows.size() > 16384) {
      return Outcome<Table>::err(fail(Errc::limit, "too many rows", line_number));
    }
  }
  if (!saw_header) {
    return Outcome<Table>::err(fail(Errc::bad_magic, "empty table", 0));
  }
  return Outcome<Table>::ok(std::move(table));
}

Outcome<void> validateTable(const Table& table) {
  if (table.columns.empty()) {
    return Outcome<void>::err(fail(Errc::state, "table has no schema", 0));
  }
  for (const auto& row : table.rows) {
    for (const auto& column : table.columns) {
      if (row.cells.find(column.name) == row.cells.end()) {
        return Outcome<void>::err(fail(Errc::state, "row missing schema column", 0));
      }
    }
  }
  return Outcome<void>::success();
}

std::string tableSummary(const Table& table) {
  std::ostringstream out;
  out << "columns=" << table.columns.size() << " rows=" << table.rows.size();
  return out.str();
}

}  // namespace rivetmesh

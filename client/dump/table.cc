/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "table.h"
#include "pattern_matcher.h"
#include <boost/algorithm/string.hpp>

using namespace Mysql::Tools::Dump;

Table::Table(uint64 id, const std::string& name, const std::string& schema,
  const std::string& sql_formatted_definition, std::vector<Field>& fields,
  std::string type, uint64 row_count, uint64 row_bound, uint64 data_lenght)
  : Abstract_plain_sql_object(id, name, schema, sql_formatted_definition),
  m_fields(fields),
  m_type(type),
  m_row_count(row_count),
  m_row_bound(row_bound),
  m_data_lenght(data_lenght)
{
  using Detail::Pattern_matcher;
  bool pk_present= false;

  std::vector<std::string> definition_lines;
  boost::split(definition_lines, sql_formatted_definition,
    boost::is_any_of("\n"), boost::token_compress_on);
  for (std::vector<std::string>::iterator it= definition_lines.begin();
    it != definition_lines.end(); ++it)
  {
    boost::trim_left(*it);
    // TODO: Look up INFORMATION_SCHEMA and get the table details.
    if (boost::starts_with(*it, "KEY ")
      || boost::starts_with(*it, "INDEX ")
      || boost::starts_with(*it, "UNIQUE KEY ")
      || boost::starts_with(*it, "UNIQUE INDEX ")
      || boost::starts_with(*it, "FULLTEXT KEY ")
      || boost::starts_with(*it, "FULLTEXT INDEX ")
      || boost::starts_with(*it, "SPATIAL KEY ")
      || boost::starts_with(*it, "SPATIAL INDEX "))
    {
      pk_present= true;
      *it= boost::algorithm::replace_last_copy(*it, "),", ")");
      m_indexes_sql_definition.push_back(*it);
    }
    else if (boost::starts_with(*it, "CONSTRAINT "))
    {
      *it= boost::algorithm::replace_last_copy(*it, ",", "");
      m_indexes_sql_definition.push_back(*it);
    }
    else
    {
      if (pk_present)
      {
        if ((it+1) == definition_lines.end())
        {
          std::string &sql_def = m_sql_definition_without_indexes;
          sql_def = boost::algorithm::replace_last_copy(sql_def, ",", "");
        }
      }
      m_sql_definition_without_indexes+= *it + '\n';
    }
  }
}

const std::string& Table::get_sql_definition_without_indexes() const
{
  return m_sql_definition_without_indexes;
}

const std::vector<std::string>& Table::get_indexes_sql_definition() const
{
  return m_indexes_sql_definition;
}

const std::vector<Field>& Table::get_fields() const
{
  return m_fields;
}

uint64 Table::get_row_data_lenght() const
{
  return m_data_lenght;
}

uint64 Table::get_row_count_bound() const
{
  return m_row_bound;
}

uint64 Table::get_row_count() const
{
  return m_row_count;
}

std::string Table::get_type() const
{
  return m_type;
}

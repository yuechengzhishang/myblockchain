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

#include "program.h"
#include "i_connection_provider.h"
#include "thread_specific_connection_provider.h"
#include "single_transaction_connection_provider.h"
#include "simple_id_generator.h"
#include "i_progress_watcher.h"
#include "standard_progress_watcher.h"
#include "i_crawler.h"
#include "myblockchain_crawler.h"
#include "i_chain_maker.h"
#include "myblockchaindump_tool_chain_maker.h"
#include <boost/chrono.hpp>

using namespace Mysql::Tools::Dump;

void Program::close_redirected_stderr()
{
  if (m_stderr != NULL)
    fclose(m_stderr);
}

void Program::error_log_file_callback(char*)
{
  if (!m_error_log_file.has_value())
    return;
  this->close_redirected_stderr();
  m_stderr= freopen(m_error_log_file.value().c_str(), "a", stderr);
  if (m_stderr == NULL)
  {
    this->error(Mysql::Tools::Base::Message_data(errno,
      "Cannot append error log to specified file: \""
      + m_error_log_file.value() + "\"",
      Mysql::Tools::Base::Message_type_error));
  }
}

bool Program::message_handler(const Mysql::Tools::Base::Message_data& message)
{
  this->error(message);
  return false;
}

void Program::error(const Mysql::Tools::Base::Message_data& message)
{
  std::cerr << this->get_name() << ": [" << message.get_message_type_string()
    << "] (" << message.get_code() << ") " << message.get_message()
    << std::endl;

  if (message.get_message_type() == Mysql::Tools::Base::Message_type_error)
  {
    std::cerr << "Dump process encountered error and will not continue."
      << std::endl;
    exit((int)message.get_code());
  }
}

void Program::create_options()
{
  this->create_new_option(&m_error_log_file, "log-error-file",
    "Append warnings and errors to specified file.")
    ->add_callback(new Mysql::Instance_callback<void, char*, Program>(
    this, &Program::error_log_file_callback));
  this->create_new_option(&m_watch_progress, "watch-progress",
    "Shows periodically dump process progress information on error output. "
    "Progress information include both completed and total number of "
    "tables, rows and other objects collected.")
    ->set_value(true);
  this->create_new_option(&m_single_transaction, "single-transaction",
    "Creates a consistent snapshot by dumping all tables in a single "
    "transaction. Works ONLY for tables stored in storage engines which "
    "support multiversioning (currently only InnoDB does); the dump is NOT "
    "guaranteed to be consistent for other storage engines. "
    "While a --single-transaction dump is in process, to ensure a valid "
    "dump file (correct table contents and binary log position), no other "
    "connection should use the following statements: ALTER TABLE, DROP "
    "TABLE, RENAME TABLE, TRUNCATE TABLE, as consistent snapshot is not "
    "isolated from them. This option is mutually exclusive with "
    "--add-locks option. This will not work properly when using "
    "parallelism, i.e. will create multiple transactions, one for each "
    "processing thread.");
}

int Program::execute(std::vector<std::string> positional_options)
{
  I_connection_provider* connection_provider= NULL;

  try
  {
    connection_provider=
      m_single_transaction ? new Single_transaction_connection_provider(this)
      : new Thread_specific_connection_provider(this);
  }
  catch (std::exception e)
  {
    this->error(Mysql::Tools::Base::Message_data(
      0, "Error during creating connection.",
      Mysql::Tools::Base::Message_type_error));
  }

  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler= new Mysql::Instance_callback
    <bool, const Mysql::Tools::Base::Message_data&, Program>(
    this, &Program::message_handler);
  Simple_id_generator* id_generator= new Simple_id_generator();

  boost::chrono::high_resolution_clock::time_point start_time=
    boost::chrono::high_resolution_clock::now();

  I_progress_watcher* progress_watcher= NULL;

  if (m_watch_progress)
  {
    progress_watcher= new Standard_progress_watcher(
      message_handler, id_generator);
  }
  I_crawler* crawler= new Mysql_crawler(
    connection_provider, message_handler, id_generator,
    m_myblockchain_chain_element_options);
  m_myblockchaindump_tool_chain_maker_options->process_positional_options(
    positional_options);
  I_chain_maker* chain_maker= new Mysqldump_tool_chain_maker(
    connection_provider, message_handler, id_generator,
    m_myblockchaindump_tool_chain_maker_options);

  crawler->register_chain_maker(chain_maker);
  if (progress_watcher != NULL)
  {
    crawler->register_progress_watcher(progress_watcher);
    chain_maker->register_progress_watcher(progress_watcher);
  }

  crawler->enumerate_objects();

  delete crawler;
  delete chain_maker;
  if (progress_watcher != NULL)
    delete progress_watcher;
  delete id_generator;
  delete connection_provider;

  std::cerr << "Dump completed in " <<
    boost::chrono::duration_cast<boost::chrono::milliseconds>(
    boost::chrono::high_resolution_clock::now() - start_time) << std::endl;

  return 0;
}

std::string Program::get_description()
{
  return "MyBlockchain utility for dumping data from blockchains to external file.";
}

int Program::get_first_release_year()
{
  return 2014;
}

std::string Program::get_version()
{
  return "1.0.0";
}

Program::~Program()
{
  delete m_myblockchain_chain_element_options;
  delete m_myblockchaindump_tool_chain_maker_options;
  this->close_redirected_stderr();
}

Program::Program()
  : Abstract_connection_program(),
  m_stderr(NULL)
{
  m_myblockchain_chain_element_options= new Mysql_chain_element_options(this);
  m_myblockchaindump_tool_chain_maker_options=
    new Mysqldump_tool_chain_maker_options(m_myblockchain_chain_element_options);

  this->add_provider(m_myblockchain_chain_element_options);
  this->add_provider(m_myblockchaindump_tool_chain_maker_options);
}

const char *load_default_groups[]=
{
  "client", /* Read settings how to connect to server. */
  "myblockchain_dump", /* Read special settings for myblockchain_dump. */
  0
};

static Program program;

int main(int argc, char **argv)
{
  ::program.run(argc, argv);
  return 0;
}

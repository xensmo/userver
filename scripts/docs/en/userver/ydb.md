 ## YDB

**Quality:** @ref QUALITY_TIERS "Platinum Tier".

YDB asynchronous driver provides interface to work with Tables, Topics and Coordination Service.

* Table (userver/ydb/table.hpp);
* Topic (userver/ydb/transaction.hpp);
* Topic writer (userver/ydb/topic_writer.hpp);
* Coordination Service (userver/ydb/coordination.hpp);

## Usage

To use YDB you have to add the component @ref ydb::YdbComponent and configure it according to the documentation.
From component you can access required client (@ref ydb::TableClient, @ref ydb::TopicClient, @ref ydb::CoordinationClient).

For writing messages to YDB topics, userver also provides @ref ydb::TopicWriter.
It can be constructed directly or obtained from @ref ydb::TopicWriterManager via @ref ydb::TopicWriterComponent.

You may store YQL queries in @ref scripts/docs/en/userver/sql_files.md.

## Examples

[Sample YDB usage](https://github.com/userver-framework/userver/tree/develop/samples/ydb_service)
[Sample YDB topic writer usage](https://github.com/userver-framework/userver/tree/develop/samples/ydb_topic_writer_service)
[YDB tests](https://github.com/userver-framework/userver/tree/develop/ydb/tests)

## More information
- https://ydb.tech/

----------

@htmlonly <div class="bottom-nav"> @endhtmlonly
⇦ @ref scripts/docs/en/userver/kafka.md |
@ref scripts/docs/en/userver/sqlite/sqlite_driver.md ⇨
@htmlonly </div> @endhtmlonly

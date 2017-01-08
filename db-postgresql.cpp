#include <stdexcept>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include "db-postgresql.h"
#include "util.h"
#include "types.h"

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

DBPostgreSQL::DBPostgreSQL(const std::string &mapdir)
{
	std::ifstream ifs((mapdir + "/world.mt").c_str());
	if(!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string const connect_string = get_setting("pgsql_connection", ifs);
	ifs.close();
	db = PQconnectdb(connect_string.c_str());

	if (PQstatus(db) != CONNECTION_OK) {
		throw std::runtime_error(std::string(
			"PostgreSQL database error: ") +
			PQerrorMessage(db)
		);
	}

	prepareStatement(
		"get_block_pos",
		"SELECT posX, posY, posZ FROM blocks"
	);
	prepareStatement(
		"get_blocks_z",
		"SELECT posX, posY, data FROM blocks WHERE posZ = $1::int4"
	);

	checkResults(PQexec(db, "START TRANSACTION;"));
	checkResults(PQexec(db, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;"));
}


DBPostgreSQL::~DBPostgreSQL()
{
	try {
		checkResults(PQexec(db, "COMMIT;"));
	} catch (std::exception& caught) {
		std::cerr << "could not finalize: " << caught.what() << std::endl;
	}
	PQfinish(db);
}

std::vector<BlockPos> DBPostgreSQL::getBlockPos()
{
	std::vector<BlockPos> positions;

	PGresult *results = execPrepared(
		"get_block_pos", 0,
		NULL, NULL, NULL, false, false
	);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row)
		positions.push_back(pg_to_blockpos(results, row, 0));

	PQclear(results);

	return positions;
}


void DBPostgreSQL::getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos)
{
	int32_t const z = htonl(zPos);

	const void *args[] = { &z };
	const int argLen[] = { sizeof(z) };
	const int argFmt[] = { 1 };

	PGresult *results = execPrepared(
		"get_blocks_z", ARRLEN(args), args,
		argLen, argFmt, false
	);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row) {
		BlockPos position;
		position.x = pg_binary_to_int(results, row, 0);
		position.y = pg_binary_to_int(results, row, 1);
		position.z = zPos;
		Block const b(
			position,
			ustring(
				reinterpret_cast<unsigned char*>(
					PQgetvalue(results, row, 2)
				),
				PQgetlength(results, row, 2)
			)
		);
		blocks[position.x].push_back(b);
	}

	PQclear(results);
}

PGresult *DBPostgreSQL::checkResults(PGresult *res, bool clear)
{
	ExecStatusType statusType = PQresultStatus(res);

	switch (statusType) {
	case PGRES_COMMAND_OK:
	case PGRES_TUPLES_OK:
		break;
	case PGRES_FATAL_ERROR:
		throw std::runtime_error(
			std::string("PostgreSQL database error: ") +
			PQresultErrorMessage(res)
		);
	default:
		throw std::runtime_error(
			"Unhandled PostgreSQL result code"
		);
	}

	if (clear)
		PQclear(res);

	return res;
}

void DBPostgreSQL::prepareStatement(const std::string &name, const std::string &sql)
{
	checkResults(PQprepare(db, name.c_str(), sql.c_str(), 0, NULL));
}

PGresult *DBPostgreSQL::execPrepared(
	const char *stmtName, const int paramsNumber,
	const void **params,
	const int *paramsLengths, const int *paramsFormats,
	bool clear, bool nobinary
)
{
	return checkResults(PQexecPrepared(db, stmtName, paramsNumber,
		(const char* const*) params, paramsLengths, paramsFormats,
		nobinary ? 1 : 0), clear
	);
}

int DBPostgreSQL::pg_to_int(PGresult *res, int row, int col)
{
	return atoi(PQgetvalue(res, row, col));
}

int DBPostgreSQL::pg_binary_to_int(PGresult *res, int row, int col)
{
	int32_t* raw = reinterpret_cast<int32_t*>(PQgetvalue(res, row, col));
	return ntohl(*raw);
}

BlockPos DBPostgreSQL::pg_to_blockpos(PGresult *res, int row, int col)
{
	BlockPos result;
	result.x = pg_to_int(res, row, col);
	result.y = pg_to_int(res, row, col + 1);
	result.z = pg_to_int(res, row, col + 2);
	return result;
}

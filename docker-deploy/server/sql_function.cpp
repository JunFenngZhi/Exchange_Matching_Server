#include "sql_function.h"
#include "exception.h"

/*
    read sql command from the file and then create tabel using connection *C.
    If fails, it will throw exception.
*/
void createTable(connection* C, string fileName) {
    string sql;
    ifstream ifs(fileName.c_str(), ifstream::in);
    if (ifs.is_open() == true) {
        string line;
        while (getline(ifs, line))
            sql.append(line);
    } else {
        throw MyException("fail to open file.");
    }

    work W(*C);
    W.exec(sql);
    W.commit();
}

/*
    Drop all the table in the DataBase. Using for test.
*/
void dropAllTable(connection* C) {
    work W(*C);
    string sql =
        "DROP TABLE IF EXISTS account;DROP TABLE IF EXISTS symbol;DROP TABLE "
        "IF EXISTS orders;";

    W.exec(sql);
    W.commit();
    cout << "Drop all the existed table" << endl;
}

/*
    insert a row into table Account. This function will throw exception when it
   fails.
*/
void addAccount(connection* C, int account_id, float balance) {
    /*TODO*/
}

/*
    insert a row into table Symbol. This function will throw exception when it
   fails.
*/
void addSymbol(connection* C, const string& sym, int account_id, int num) {
    /*TODO*/
}
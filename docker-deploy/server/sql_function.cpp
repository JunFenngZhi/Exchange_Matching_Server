#include "sql_function.h"

#include <assert.h>

#include "exception.h"

/*
    read sql command from the file and then create tabel using connection *C.
    If fails, it will throw exception.
*/
void createTable(connection * C, string fileName) {
  string sql;
  ifstream ifs(fileName.c_str(), ifstream::in);
  if (ifs.is_open() == true) {
    string line;
    while (getline(ifs, line))
      sql.append(line);
  }
  else {
    throw MyException("fail to open file.");
  }

  work W(*C);
  W.exec(sql);
  W.commit();
}

/*
    Drop all the table in the DataBase. Using for test.
*/
void dropAllTable(connection * C) {
  work W(*C);
  string sql = "DROP TABLE IF EXISTS symbol;DROP TABLE IF EXISTS account;DROP TABLE "
               "IF EXISTS orders;";

  W.exec(sql);
  W.commit();
  cout << "Drop all the existed table" << endl;
}

/*
    insert a row into table Account. This function will throw exception when it
    fails.
*/
void addAccount(connection * C, int account_id, float balance) {
  work W(*C);
  stringstream sql;
  sql << "INSERT INTO ACCOUNT(ACCOUNT_ID, BALANCE) VALUES(" << account_id << ","
      << balance << ");";
  W.exec(sql.str());
  W.commit();
}

/*
    Insert a new row into table Symbol. If account does not exist, it will fail
   and throw exception. If the SYM already exist in this account, it will update
   the old value.
   */
void addSymbol(connection * C, const string & sym, int account_id, int num) {
  /*
    Attribute "id"="account_id" 可能setattribute的时候要转成str
    */
  work W(*C);
  stringstream sql;
  sql << "INSERT INTO SYMBOL(ACCOUNT_ID, SYM, AMOUNT) VALUES(" << account_id << ","
      << W.quote(sym) << "," << num
      << ") ON CONFLICT ON CONSTRAINT symbol_pk DO UPDATE SET AMOUNT = " << num << "+"
      << "SYMBOL.AMOUNT"
      << ";";
  W.exec(sql.str());
  W.commit();
}

/*
    if current order is a sell order, get all buy orders for the same
   symbol.(price ascending sort) if current order is a buy order, get all sell
   orders for the same symbol.(price descending sort) this function will set all
   the eligible orders into lock status(RWlock).
*/
result getEligibleOrders(connection * C, const string & sym, int amount, float limit) {
  nontransaction N(*C);
  stringstream sql;
  // match order type(buy/sell), limit price and state(open), ordered by
  // time(old->new)
  if (amount > 0) {
    // Buy
    sql << "SELECT * FROM ORDERS WHERE SYM=" << N.quote(sym)
        << " AND AMOUNT<0 AND LIMIT_PRICE<=" << limit
        << "AND STATE=\'open\' ORDER BY LIMIT_PRICE DESC, TIME ASC;";
  }
  else {
    // Sell
    sql << "SELECT * FROM ORDERS WHERE SYM=" << N.quote(sym)
        << " AND AMOUNT>0 AND LIMIT_PRICE>=" << limit
        << "AND STATE=\'open\' ORDER BY LIMIT_PRICE ASC, TIME ASC;";
  }
  result R(N.exec(sql.str()));
  return R;
}

/*
   Insert an order into table ORDERS. 
   If trans_id is -1, function will insert a new row without specifying the trans_id, and then send 
   back the generated trans_id through reference parameter int& trans_id.
   If trans_id is not -1, function will insert a new row using the given trans_id. 
*/
void addOrder(connection * C,
              int & trans_id,
              int amount,
              float limit,
              int account_id,
              const string & sym,
              string state) {
  work W(*C);
  stringstream sql;
  assert(trans_id >= -1);
  if (trans_id != -1) {
    sql << "INSERT INTO ORDERS(TRANS_ID, ACCOUNT_ID, SYM, AMOUNT, LIMIT_PRICE, STATE, "
           "TIME) VALUES("
        << trans_id << account_id << "," << W.quote(sym) << "," << amount << "," << limit
        << "," << W.quote(state) << ",NOW());";
    W.exec(sql.str());
    W.commit();
  }
  else {
    //TODO: 不指定trans_id， 并且将数据库生成的trans_id赋值给参数 int& trans_id.
  }
}

/*
    reduce the monry or symbol from the corresponding account based on order
   info.
*/
void reduceMoneyOrSymbol(connection * C,
                         const string & sym,
                         int account_id,
                         int amount,
                         float limit) {
  work W(*C);
  stringstream sql;
  if (amount > 0) {
    // Buy: deduct money from buyer account
    sql << "UPDATE ACCOUNT set BALANCE=ACCOUNT.BALANCE-" << amount * limit
        << " WHERE ACCOUNT_ID=" << account_id << ";";
  }
  else {
    // Sell: deduct symbol from seller account
    sql << "UPDATE SYMBOL set AMOUNT=SYMBOL.AMOUNT+" << amount
        << " WHERE ACCOUNT_ID=" << account_id << "AND SYM=" << W.quote(sym) << ";";
  }
  W.exec(sql.str());
  W.commit();
}

/*
    set the specific OPEN order EXECUTED. This function will throw exception if
   order's version is changed. If succeed, it will update this order's version.
*/
void setOrderExecuted(connection * C,
                      int o_trans_id,
                      const string & o_time,
                      int o_version) {
  work W(*C);
  string sql = "UPDATE ORDERS SET STATE = " + W.quote("executed") +
               ", VERSION = " + to_string(o_version + 1) +
               " WHERE TRANS_ID = " + to_string(o_trans_id) +
               " AND TIME = " + W.quote(o_time) +
               " AND VERSION = " + to_string(o_version) + ";";
  W.exec(sql.c_str());
  W.commit();
}

/*
    update the amount of the specific OPEN order.This function will throw
   exception if order version is changed. If succeed, it will update this
   order's version.
*/
void updateOpenOrder(connection * C,
                     int o_remain_amount,
                     int o_trans_id,
                     const string & o_time,
                     int o_version) {
  work W(*C);
  string sql = "UPDATE ORDERS SET AMONUT = " + to_string(o_remain_amount) +
               ", VERSION = " + to_string(o_version + 1) +
               " WHRER TRANS_ID = " + to_string(o_trans_id) +
               " AND TIME = " + W.quote(o_time) +
               " AND VERSION = " + to_string(o_version) + ";";
  W.exec(sql.c_str());
  W.commit();
}

/*
Given a trans_id, search for all the orders with this trans_id in the orders
table. Return the results as (state, amount, price, time). There might be 1 open
+ multiple executed, or 1 canceled + multiple executed.
*/
result searchOrders(connection * C, int trans_id) {
  nontransaction N(*C);
  stringstream sql;
  sql << "SELECT STATE, AMOUNT, LIMIT_PRICE, TIME FROM ORDERS WHERE TRANS_ID=" << trans_id
      << ";";
  result R(N.exec(sql.str()));
  return R;
}

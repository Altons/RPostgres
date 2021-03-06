#ifndef __RPOSTGRES_PQ_ROW__
#define __RPOSTGRES_PQ_ROW__

#include <Rcpp.h>
#include <libpq-fe.h>
#include <boost/noncopyable.hpp>

// PqRow -----------------------------------------------------------------------
// A single row of results from PostgreSQL
class PqRow : boost::noncopyable {
  PGresult* pRes_;

public:

  PqRow(PGconn* conn) {
    if (conn == NULL)
      return;
    pRes_ = PQgetResult(conn);

    // We're done, but we need to call PQgetResult until it returns NULL
    if (status() == PGRES_TUPLES_OK) {
      PGresult* next = PQgetResult(conn);
      while(next != NULL) {
        PQclear(next);
        next = PQgetResult(conn);
      }
    }

    if (pRes_ == NULL) {
      PQclear(pRes_);
      Rcpp::stop("No active query");
    }

    if (PQresultStatus(pRes_) == PGRES_FATAL_ERROR) {
      PQclear(pRes_);
      Rcpp::stop(PQerrorMessage(conn));
    }
  }

  ExecStatusType status() {
    return PQresultStatus(pRes_);
  }

  bool hasData() {
    return status() == PGRES_SINGLE_TUPLE;
  }

  ~PqRow() {
    try {
      PQclear(pRes_);
    } catch(...) {}
  }

  int rowsAffected() {
    return atoi(PQcmdTuples(pRes_));
  }

  Rcpp::List exception_info() {
    const char* sev = PQresultErrorField(pRes_, PG_DIAG_SEVERITY);
    const char* msg = PQresultErrorField(pRes_, PG_DIAG_MESSAGE_PRIMARY);
    const char* det = PQresultErrorField(pRes_, PG_DIAG_MESSAGE_DETAIL);
    const char* hnt = PQresultErrorField(pRes_, PG_DIAG_MESSAGE_HINT);

    return Rcpp::List::create(
      Rcpp::_["severity"] = sev == NULL ? "" : std::string(sev),
      Rcpp::_["message"]  = msg == NULL ? "" : std::string(msg),
      Rcpp::_["detail"]   = det == NULL ? "" : std::string(det),
      Rcpp::_["hint"]     = hnt == NULL ? "" : std::string(hnt)
    );
  }

  // Value accessors -----------------------------------------------------------
  bool value_null(int j) {
    return PQgetisnull(pRes_, 0, j);
  }

  int value_int(int j) {
    return value_null(j) ? NA_INTEGER : atoi(PQgetvalue(pRes_, 0, j));
  }

  double value_double(double j) {
    return value_null(j) ? NA_REAL : atof(PQgetvalue(pRes_, 0, j));
  }

  SEXP value_string(int j) {
    if (value_null(j))
      return NA_STRING;

    char* val = PQgetvalue(pRes_, 0, j);
    return Rf_mkCharCE(val, CE_UTF8);
  }

  SEXP value_raw(int j) {
    int size = PQgetlength(pRes_, 0, j);
    const void* blob = PQgetvalue(pRes_, 0, j);

    SEXP bytes = Rf_allocVector(RAWSXP, size);
    memcpy(RAW(bytes), blob, size);

    return bytes;
  }

  int value_logical(int j) {
    return value_null(j) ? NA_LOGICAL :
      (strcmp(PQgetvalue(pRes_, 0, j), "t") == 0);
  }

  void set_list_value(SEXP x, int i, int j) {
    switch(TYPEOF(x)) {
    case LGLSXP:
      LOGICAL(x)[i] = value_logical(j);
      break;
    case INTSXP:
      INTEGER(x)[i] = value_int(j);
      break;
    case REALSXP:
      REAL(x)[i] = value_double(j);
      break;
    case STRSXP:
      SET_STRING_ELT(x, i, value_string(j));
      break;
    case VECSXP:
      SET_VECTOR_ELT(x, i, value_raw(j));
      break;
    }
  }

};

#endif

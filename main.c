#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

const char *DELETE_STATEMENT = "DELETE FROM expenses WHERE id = ?;";

const char *QUERY_EXPENSES_STATEMENT =
    "SELECT title, amount, expense_date, category, "
    "CASE CAST(strftime('%w', expense_date) AS INTEGER)"
    "WHEN 0 THEN 'Sunday'"
    "WHEN 1 THEN 'Monday'"
    "WHEN 2 THEN 'Tuesday'"
    "WHEN 3 THEN 'Wednesday'"
    "WHEN 4 THEN 'Thursday'"
    "WHEN 5 THEN 'Friday'"
    "ELSE 'Saturday'"
    "END AS day_of_week,"
    "id"
    " FROM expenses ORDER BY expense_date;";

const char *CREATE_STATEMENT =
    "CREATE TABLE IF NOT EXISTS expenses ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "title TEXT,"
    "amount FLOAT,"
    "expense_date TEXT NOT NULL CHECK(date(expense_date) = expense_date),"
    "category TEXT"
    ");";

const char *INSERT_STATEMENT = "INSERT INTO expenses ("
                               "title,"
                               "amount,"
                               "expense_date,"
                               "category"
                               ") VALUES (?,?,?,?);";

const char *PROGRAM_NAME = "c-expense";

void to_lower_case(char *value) {
  for (int i = 0; value[i] != '\0'; i++) {
    value[i] = tolower((unsigned char)value[i]);
  }
}

int is_option_token(const char *value) {
  return strcmp(value, "-t") == 0 || strcmp(value, "--title") == 0 ||
         strcmp(value, "-a") == 0 || strcmp(value, "--amount") == 0 ||
         strcmp(value, "-c") == 0 || strcmp(value, "--category") == 0 ||
         strcmp(value, "-d") == 0 || strcmp(value, "--date") == 0 ||
         strcmp(value, "-i") == 0 || strcmp(value, "--id") == 0;
}

char *get_text_from_arguments(int total_arguments, char *variables[],
                              char *keyword_short, char *keyword_long,
                              int required) {
  for (int i = 0; i < total_arguments; i++) {
    int long_match = strcmp(variables[i], keyword_long) == 0;
    int short_match = strcmp(variables[i], keyword_short) == 0;

    if (long_match || short_match) {
      if (variables[i + 1] == NULL || is_option_token(variables[i + 1])) {
        fprintf(stderr, "%s was not provided.\n", keyword_long);
        return NULL;
      }

      return variables[i + 1];
    }
  }

  if (required == 1) {
    fprintf(stderr, "Could not find value for keyword %s in arguments.\n",
            keyword_long);
  }

  return NULL;
}

int handle_insert(int total_arguments, char *variables[], sqlite3 *db) {
  char *titleInput =
      get_text_from_arguments(total_arguments, variables, "-t", "--title", 1);

  char *amountInput =
      get_text_from_arguments(total_arguments, variables, "-a", "--amount", 1);

  // check for NULL in title and amount
  if (titleInput == NULL) {
    fprintf(stderr, "title was not provided.\n");
    return 1;
  }

  if (amountInput == NULL) {
    fprintf(stderr, "amount was not provided.\n");
    return 1;
  }

  char *endptr;
  errno = 0;
  double amount = strtod(amountInput, &endptr);

  if (errno == ERANGE || !isfinite(amount) || endptr == amountInput ||
      *endptr != '\0') {
    fprintf(stderr, "Invalid amount provided.\n");
    return 1;
  }

  char *categoryInput = get_text_from_arguments(total_arguments, variables,
                                                "-c", "--category", 0);

  char *dateInput =
      get_text_from_arguments(total_arguments, variables, "-d", "--date", 0);

  // fallback date parameters
  time_t currentTime;
  time(&currentTime);
  struct tm *localTime = localtime(&currentTime);
  int defaultYear = localTime->tm_year + 1900;
  int defaultMonth = localTime->tm_mon + 1;
  int defaultDay = localTime->tm_mday;
  char dateString[50] = "";

  if (dateInput == NULL) {
    snprintf(dateString, sizeof(dateString), "%04d-%02d-%02d", defaultYear,
             defaultMonth, defaultDay);
  } else {
    char *monthString = strtok(dateInput, "/");
    char *dayString = strtok(NULL, "/");
    char *yearString = strtok(NULL, "/");

    if (monthString == NULL) {
      fprintf(stderr, "month was not provided.\n");
      return 1;
    }

    if (dayString == NULL) {
      fprintf(stderr, "day was not provided.\n");
      return 1;
    }

    // sanitize month input
    long monthValue;

    errno = 0;
    monthValue = strtol(monthString, &endptr, 10);

    if (errno == ERANGE || endptr == monthString || *endptr != '\0' ||
        monthValue < 1 || monthValue > 12) {
      fprintf(stderr, "Error: Invalid month (must be 1-12).\n");
      return 1;
    }

    // 2. sanitize day
    errno = 0;
    long dayVal;

    dayVal = strtol(dayString, &endptr, 10);

    if (errno == ERANGE || endptr == dayString || *endptr != '\0' ||
        dayVal < 1 || dayVal > 31) {
      fprintf(stderr, "Error: Invalid day (must be 1-31).\n");
      return 1;
    }

    // 3. sanitize optional year
    long yearVal;

    if (yearString != NULL) {
      errno = 0;
      yearVal = strtol(yearString, &endptr, 10);

      if (errno == ERANGE || endptr == yearString || *endptr != '\0' ||
          yearVal < 1000 || yearVal > 9999) {
        fprintf(stderr, "Error: Invalid year (must be a 4-digit number).\n");
        return 1;
      }
    } else {
      yearVal = defaultYear; // Fallback to current year
    }

    // Print structurally verified ISO date
    snprintf(dateString, sizeof(dateString), "%04ld-%02ld-%02ld", yearVal,
             monthValue, dayVal);
  }

  char category[50] = "no category";

  if (categoryInput != NULL) {
    snprintf(category, sizeof(category), "%s", categoryInput);
  }

  // prepare sqlite statement and execution
  sqlite3_stmt *statement;

  if (sqlite3_prepare_v2(db, INSERT_STATEMENT, -1, &statement, NULL) !=
      SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));

    return 1;
  }

  sqlite3_bind_text(statement, 1, titleInput, -1, SQLITE_STATIC);
  sqlite3_bind_double(statement, 2, amount);
  sqlite3_bind_text(statement, 3, dateString, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 4, category, -1, SQLITE_STATIC);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  sqlite3_finalize(statement);

  printf("Successfully inserted expense %s at %s on %s type %s.\n", titleInput,
         amountInput, dateString, categoryInput ? categoryInput : "n/a");

  return 0;
}

int create_directory(const char *path) {
  if (mkdir(path, 0755) == -1 && errno != EEXIST) {
    fprintf(stderr, "Error creating ~/.config directory.\n");
    return 1;
  }

  return 0;
}

int ensure_directory_exists(const char *homeDir) {
  char path[512];

  // ensure ~/.config exists
  snprintf(path, sizeof(path), "%s/.config", homeDir);

  create_directory(path);

  // ensure ~/.config/c-expense exists
  snprintf(path, sizeof(path), "%s/.config/c-expense", homeDir);

  create_directory(path);

  return 0;
}

int initialize_db(sqlite3 **db, char **zErrMsg) {
  const char *homeDir = getenv("HOME");

  if (homeDir == NULL) {
    fprintf(stderr, "Error: Could not find the HOME environment variable.\n");
    return 1;
  }

  if (ensure_directory_exists(homeDir) != 0) {
    return 1;
  }

  char dbPath[512];

  snprintf(dbPath, sizeof(dbPath), "%s/.config/c-expense/db.sqlite3", homeDir);

  int connection = sqlite3_open(dbPath, db);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(*db));

    return 1;
  }

  // attempt to create expense table if not exist.
  connection = sqlite3_exec(*db, CREATE_STATEMENT, NULL, NULL, zErrMsg);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "Failed to create expense table: %s\n",
            sqlite3_errmsg(*db));
    sqlite3_free(*zErrMsg);
    sqlite3_close(*db);
    return 1;
  }

  return 0;
}

int print_total(double total) {
  printf("---------------------------------------------------------------------"
         "----------\n");
  printf("%-37s %.2f\n", "Total:", total);
  printf("---------------------------------------------------------------------"
         "----------\n");

  return 0;
}

int list_expenses(sqlite3 *db) {
  int connection;

  // retrieve data from db
  sqlite3_stmt *statement;

  connection =
      sqlite3_prepare_v2(db, QUERY_EXPENSES_STATEMENT, -1, &statement, NULL);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s.\n", sqlite3_errmsg(db));
    return 1;
  }

  printf("%-10s %-5s %-20s %-10s %-12s %-15s\n", "Day", "ID", "Title", "Amount",
         "Date", "Category");
  printf("%-10s %-5s %-20s %-10s %-12s %-15s\n", "----------", "-----",
         "--------------------", "----------", "------------",
         "---------------");

  int rowCount = 0;
  float totalExpense = 0;
  char lastDayBuffer[32] = ""; // stores name of day

  while ((connection = sqlite3_step(statement)) == SQLITE_ROW) {
    const unsigned char *title = sqlite3_column_text(statement, 0);
    const unsigned char *amount = sqlite3_column_text(statement, 1);
    const unsigned char *date = sqlite3_column_text(statement, 2);
    const unsigned char *category = sqlite3_column_text(statement, 3);
    const unsigned char *day_of_week = sqlite3_column_text(statement, 4);
    const unsigned char *id = sqlite3_column_text(statement, 5);

    title = title ? title : (const unsigned char *)"";
    amount = amount ? amount : (const unsigned char *)"";
    date = date ? date : (const unsigned char *)"";
    category = category ? category : (const unsigned char *)"";
    day_of_week = day_of_week ? day_of_week : (const unsigned char *)"";
    id = id ? id : (const unsigned char *)"";

    const char *dayToPrint = "";

    // print by day of week
    int differentDay = strcmp((const char *)day_of_week, lastDayBuffer) != 0;

    // if different day detected, set last day to current day
    if (rowCount == 0 || differentDay) {
      if (lastDayBuffer[0] != '\0') {
        print_total(totalExpense);
        totalExpense = 0;
      }

      dayToPrint = (const char *)day_of_week;

      strncpy(lastDayBuffer, (const char *)day_of_week,
              sizeof(lastDayBuffer) - 1);
    }

    printf("%-10s %-5s %-20s %-10.2f %-12s %-15s\n", dayToPrint, id, title,
           atof((char *)amount), date, category);
    totalExpense += atof((char *)amount);
    rowCount++;
  }

  if (rowCount > 0) {
    print_total(totalExpense);
  }

  totalExpense = 0;

  if (rowCount == 0) {
    printf("Reached the end of results.\n");
  }

  if (connection != SQLITE_DONE) {
    fprintf(stderr, "Execution error: %s.\n", sqlite3_errmsg(db));
  }

  sqlite3_finalize(statement);

  return 0;
}

int delete_expense(sqlite3 *db, int total_arguments, char *variables[]) {
  int connection;
  char *idInput =
      get_text_from_arguments(total_arguments, variables, "-i", "--id", 1);

  if (idInput == NULL) {
    return 1;
  }

  char *endptr;
  errno = 0;
  long sanitizedIdInput = strtol(idInput, &endptr, 10);

  if (errno == ERANGE || endptr == idInput || *endptr != '\0') {
    fprintf(stderr, "Error: Invalid expense ID provided.\n");
    return 1;
  }

  sqlite3_stmt *statement;

  connection = sqlite3_prepare_v2(db, DELETE_STATEMENT, -1, &statement, NULL);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "%s\n", sqlite3_errmsg(db));
    return 1;
  }

  sqlite3_bind_int64(statement, 1, sanitizedIdInput);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(statement);
    return 1;
  }

  sqlite3_finalize(statement);

  if (sqlite3_changes(db) == 0) {
    fprintf(stderr, "Error: Record with ID %ld does not exist.\n",
            sanitizedIdInput);

    // return error state because no deletion occurred
    return 1;
  }

  printf("Successfully deleted record id %ld\n", sanitizedIdInput);
  return 0;
}

int print_help() {
  printf("Usage: %s <command> [options]\n"
         "\n"
         "Commands:\n"
         " add Add a new expense\n"
         " delete Delete an expense by ID\n"
         " list List all expenses\n"
         " help Show this help message\n"
         "\n"
         "Add options:\n"
         " -t, --title TITLE Expense title (required)\n"
         " -a, --amount AMOUNT Amount, e.g. 10 or 10.50 (required)\n"
         " -c, --category CAT Category, e.g. food, transport (optional)\n"
         " -d, --date DATE Date DD/MM or DD/MM/YYYY, e.g. 10/09 (optional)\n"
         "\n"
         "Delete options:\n"
         " -i, --id ID ID of expense to delete (required)\n"
         "\n"
         "Examples:\n"
         " %s delete -i 10\n"
         " deletes an expense with id 10.\n"
         "\n"
         " %s add --title lunch --amount 10 --category food --date 10/09\n"
         " %s add -t lunch -a 10 -c food -d 10/09\n"
         " adds a lunch expense.\n"
         "\n"
         " %s list\n"
         " lists all expenses.\n"
         "\n",
         PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME);

  return 0;
}

int main(int total_arguments, char *variables[]) {
  if (total_arguments < 2) {
    return print_help();
  }

  sqlite3 *db = NULL;
  char *zErrMsg = NULL;

  if (initialize_db(&db, &zErrMsg) != 0) {
    return 1;
  }

  char *command = variables[1];

  to_lower_case(command);

  int status_code = 0;

  if (strcmp(command, "add") == 0) {
    status_code = handle_insert(total_arguments, variables, db);
  } else if (strcmp(command, "list") == 0) {
    status_code = list_expenses(db);
  } else if (strcmp(command, "delete") == 0) {
    status_code = delete_expense(db, total_arguments, variables);
  } else {
    fprintf(stderr, "Invalid command received: %s.\n", command);
    status_code = 1;
  }

  sqlite3_free(zErrMsg);
  sqlite3_close(db);
  return status_code;
}

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

const char *DELETE_STATEMENT = "DELETE FROM expenses WHERE id = ?;";

const char *QUERY_CURRENT_MONTH_EXPENSES_STATEMENT =
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
    "id "
    "FROM expenses "
    "WHERE strftime('%Y-%m', expense_date) = (SELECT strftime('%Y-%m', 'now', "
    "'localtime')) "
    "ORDER BY expense_date;";

const char *QUERY_CURRENT_YEAR_EXPENSES_STATEMENT =
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
    "id "
    "FROM expenses "
    "WHERE strftime('%Y', expense_date) = (SELECT strftime('%Y', 'now', "
    "'localtime')) "
    "ORDER BY expense_date;";

const char *QUERY_CURRENT_DAY_EXPENSES_STATEMENT =
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
    "id "
    "FROM expenses "
    "WHERE expense_date = date('now', 'localtime')"
    "ORDER BY expense_date;";

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
    // explicitly cast the int returned by tolower back to char
    value[i] = (char)tolower((unsigned char)value[i]);
  }
}

bool is_option_token(const char *value) {
  return (strcmp(value, "-t") == 0 || strcmp(value, "--title") == 0 ||
          strcmp(value, "-a") == 0 || strcmp(value, "--amount") == 0 ||
          strcmp(value, "-c") == 0 || strcmp(value, "--category") == 0 ||
          strcmp(value, "-d") == 0 || strcmp(value, "--date") == 0 ||
          strcmp(value, "-m") == 0 || strcmp(value, "--mode") == 0 ||
          strcmp(value, "-h") == 0 || strcmp(value, "--help") == 0 ||
          strcmp(value, "-i") == 0 || strcmp(value, "--id") == 0) != 0;
}

char *get_text_from_arguments(int total_arguments, char *variables[],
                              const char *keyword_short,
                              const char *keyword_long, int required) {
  for (int i = 0; i < total_arguments; i++) {
    bool longMatch = strcmp(variables[i], keyword_long) == 0;
    bool shortMatch = strcmp(variables[i], keyword_short) == 0;

    if (longMatch || shortMatch) {
      if (variables[i + 1] == NULL || is_option_token(variables[i + 1])) {
        fprintf(stderr, "%s was not provided.\n", keyword_long);
        exit(EXIT_FAILURE);
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

int execute_insert_expense(const char *category, double amount,
                           const char *title, sqlite3 *database,
                           char dateString[50]) {
  // prepare sqlite statement and execution
  sqlite3_stmt *statement;

  if (sqlite3_prepare_v2(database, INSERT_STATEMENT, -1, &statement, NULL) !=
      SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n",
            sqlite3_errmsg(database));

    return 1;
  }

  sqlite3_bind_text(statement, 1, title, -1, SQLITE_STATIC);
  sqlite3_bind_double(statement, 2, amount);
  sqlite3_bind_text(statement, 3, dateString, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 4, category, -1, SQLITE_STATIC);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(database));
    return 1;
  }

  sqlite3_finalize(statement);

  printf("Successfully inserted expense %s at %.2f on %s type %s.\n", title,
         amount, dateString, (category != NULL) ? category : "n/a");

  return 0;
}

int handle_insert(int total_arguments, char *variables[], sqlite3 *database) {
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

  int execute_result = execute_insert_expense(category, amount, titleInput,
                                              database, dateString);

  if (execute_result == 1) {
    return 1;
  }

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

int initialize_db(sqlite3 **database, char **zErrMsg) {
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

  int connection = sqlite3_open(dbPath, database);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(*database));

    return 1;
  }

  // attempt to create expense table if not exist.
  connection = sqlite3_exec(*database, CREATE_STATEMENT, NULL, NULL, zErrMsg);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "Failed to create expense table: %s\n",
            sqlite3_errmsg(*database));
    sqlite3_free(*zErrMsg);
    sqlite3_close(*database);
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

void truncate_with_ellipsis(char *dest, size_t dest_size, const char *src,
                            size_t max_len) {
  size_t src_len = strlen(src);

  if (src_len <= max_len) {
    snprintf(dest, dest_size, "%s", src);
  } else {
    // Leave room for the 3 dots (...) and null terminator ('\0')
    size_t copy_len = (max_len > 3) ? (max_len - 3) : 0;
    snprintf(dest, dest_size, "%.*s...", (int)copy_len, src);
  }
}

int list_expenses(int total_arguments, char *variables[], sqlite3 *database) {
  int connection;

  // retrieve data from database
  sqlite3_stmt *statement;

  // check if list all.
  // list current day by default.
  const char *mode =
      get_text_from_arguments(total_arguments, variables, "-m", "--mode", 0);

  const char *query = QUERY_CURRENT_DAY_EXPENSES_STATEMENT;

  if (mode != NULL) {
    if (strcmp(mode, "month") == 0) {
      query = QUERY_CURRENT_MONTH_EXPENSES_STATEMENT;
    } else if (strcmp(mode, "day") == 0) {
      query = QUERY_CURRENT_DAY_EXPENSES_STATEMENT;
    } else if (strcmp(mode, "year") == 0) {
      query = QUERY_CURRENT_YEAR_EXPENSES_STATEMENT;
    } else {
      fprintf(stderr, "Error: Invalid mode %s.\n", mode);
      return 1;
    }
  }

  connection = sqlite3_prepare_v2(database, query, -1, &statement, NULL);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s.\n",
            sqlite3_errmsg(database));
    return 1;
  }

  printf("%-10s %-5s %-20s %-10s %-12s %-15s\n", "Day", "ID", "Title", "Amount",
         "Date", "Category");
  printf("%-10s %-5s %-20s %-10s %-12s %-15s\n", "----------", "-----",
         "--------------------", "----------", "------------",
         "---------------");

  int rowCount = 0;
  double totalExpense = 0;
  char lastDayBuffer[32] = ""; // stores name of day

  while ((connection = sqlite3_step(statement)) == SQLITE_ROW) {
    const unsigned char *title = sqlite3_column_text(statement, 0);
    const double amount = sqlite3_column_double(statement, 1);
    const unsigned char *date = sqlite3_column_text(statement, 2);
    const unsigned char *category = sqlite3_column_text(statement, 3);
    const unsigned char *day_of_week = sqlite3_column_text(statement, 4);
    const int expenseId = sqlite3_column_int(statement, 5);

    title = (title != NULL) ? title : (const unsigned char *)"";
    date = (date != NULL) ? date : (const unsigned char *)"";
    category = (category != NULL) ? category : (const unsigned char *)"";
    day_of_week =
        (day_of_week != NULL) ? day_of_week : (const unsigned char *)"";

    const char *dayToPrint = "";

    // print by day of week
    int differentDay = strcmp((const char *)day_of_week, lastDayBuffer);

    // if different day detected, set last day to current day
    if (rowCount == 0 || differentDay != 0) {
      if (lastDayBuffer[0] != '\0') {
        print_total(totalExpense);
        totalExpense = 0;
      }

      dayToPrint = (const char *)day_of_week;

      strncpy(lastDayBuffer, (const char *)day_of_week,
              sizeof(lastDayBuffer) - 1);
    }

    char truncatedTitle[20];

    truncate_with_ellipsis(truncatedTitle, sizeof(truncatedTitle),
                           (char *)title, 20);

    printf("%-10s %-5d %-20s %-10.2f %-12s %-15s\n", dayToPrint, expenseId,
           truncatedTitle, amount, date, category);
    totalExpense += amount;
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
    fprintf(stderr, "Execution error: %s.\n", sqlite3_errmsg(database));
  }

  sqlite3_finalize(statement);

  return 0;
}

int delete_expense(sqlite3 *database, int total_arguments, char *variables[]) {
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

  connection =
      sqlite3_prepare_v2(database, DELETE_STATEMENT, -1, &statement, NULL);

  if (connection != SQLITE_OK) {
    fprintf(stderr, "%s\n", sqlite3_errmsg(database));
    return 1;
  }

  sqlite3_bind_int64(statement, 1, sanitizedIdInput);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(database));
    sqlite3_finalize(statement);
    return 1;
  }

  sqlite3_finalize(statement);

  if (sqlite3_changes(database) == 0) {
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
         " list List expenses. Returns current day by default.\n"
         " help Show this help message\n"
         "\n"
         "Add options:\n"
         " -t, --title TITLE Expense title (required)\n"
         " -a, --amount AMOUNT Amount, e.g. 10 or 10.50 (required)\n"
         " -c, --category CAT Category, e.g. food, transport (optional)\n"
         " -d, --date DATE Date MM/DD or MM/DD/YYYY, e.g. 10/09 (optional)\n"
         "\n"
         "Delete options:\n"
         " -i, --id ID ID of expense to delete (required)\n"
         "\n"
         "List options:\n"
         " -m, --mode day, month, year (optional)\n"
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
         " lists all current day expenses.\n"
         " %s list --mode month\n"
         " lists all current month expenses.\n"
         " %s list --mode year\n"
         " lists all current year expenses.\n"
         "\n",
         PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME,
         PROGRAM_NAME, PROGRAM_NAME);

  return 0;
}

int main(int total_arguments, char *variables[]) {
  if (total_arguments < 2) {
    return print_help();
  }

  sqlite3 *database = NULL;
  char *zErrMsg = NULL;

  if (initialize_db(&database, &zErrMsg) != 0) {
    return 1;
  }

  char *command = variables[1];

  to_lower_case(command);

  int status_code = 0;

  if (strcmp(command, "add") == 0) {
    status_code = handle_insert(total_arguments, variables, database);
  } else if (strcmp(command, "help") == 0) {
    print_help();
  } else if (strcmp(command, "list") == 0) {
    status_code = list_expenses(total_arguments, variables, database);
  } else if (strcmp(command, "delete") == 0) {
    status_code = delete_expense(database, total_arguments, variables);
  } else {
    fprintf(stderr, "Invalid command received: %s.\n", command);
    status_code = 1;
  }

  sqlite3_free(zErrMsg);
  sqlite3_close(database);
  return status_code;
}

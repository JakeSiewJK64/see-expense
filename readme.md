## Description

Small C project to help me track my own expenses. Tired of using spreadsheet to write my expenditure and manually writing formulas to calculate.

## How it works

```
NAME
    c-expense - simple CLI expense tracker

SYNOPSIS
    c-expense <list, add, delete> [options]

COMMANDS
    add Add a new expense
    delete Delete an expense by ID
    list List all expenses

    add options:
      -t, --title TITLE Expense title (required)
      -a, --amount AMOUNT Amount, e.g. 10.50 (required)
      -c, --category CAT Category, e.g. food (required)
      -d, --date DATE Date DD/MM or DD/MM/YYYY, e.g. 10/09 (required)

    delete options:
      -i, --id ID ID of expense to delete (required)

EXAMPLES
    c-expense delete -i 10
        deletes expense with id 10.

    c-expense add --title lunch --amount 10 --category food --date 10/09
    c-expense add -t lunch -a 10 -c food -d 10/09
        adds a lunch expense.

    c-expense list
        lists all expenses.
```

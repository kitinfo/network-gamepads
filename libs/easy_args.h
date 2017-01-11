#pragma once
#include <stdbool.h>

/*
 * Adds an argument to the parsing list.
 *
 * @param char* argShort
 * 	Short version for the identifier (example: "-d"). NULL value means no checking.
 * @param char* argLong
 * 	Long version of the identifier (example: "--debug"). NULL value means no checking.
 * @param void* func
 * 	Callback function which will called when one of the identifiers is found.
 *	Signature must be:
 *
 * 	"int name(int argc, char* argv[])".
 *
 *	Arguments:
 *		int argc => size of argv
 *		char* argv[] => arguments with argv[0] as identifier
 *
 *	Return: int
 *		The callback function should return a value less than
 *		zero when the argument is not in well formated or other
 *		things gone wrong.
 *
 * @param unsigned arguments
 * 	Defines how many arguments the identifier wants.
 * 	If there are not enough arguments an error will be printed.
 */
int eargs_addArgument(char* argShort, char* argLong, void* func, unsigned arguments);

/**
 * Adds an int argument to the parsing list. The value will be parsed with strtol() with no error checking.
 * @param char* argShort
 *  Short version for the identifier (example: "-d"). NULL value means no checking.
 * @param char* argLong
 *  Long version of the identifier (example: "--debug"). NULL value means no checking.
 * @param int* container
 *  Pointer to the target int.
 */
int eargs_addArgumentInt(char* argShort, char* argLong, int* container);

/**
 * Adds an int argument to the parsing list. The value will be parsed with strtoul() with no error checking.
 * @param char* argShort
 *  Short version for the identifier (example: "-d"). NULL value means no checking.
 * @param char* argLong
 *  Long version of the identifier (example: "--debug"). NULL value means no checking.
 * @param unsigned* container
 *  Pointer to the target unsigned int.
 */
int eargs_addArgumentUInt(char* argShort, char* argLong, unsigned* container);

/**
 * Adds an int argument to the parsing list. The value will be parsed with strtol() with no error checking.
 * @param char* argShort
 *  Short version for the identifier (example: "-d"). NULL value means no checking.
 * @param char* argLong
 *  Long version of the identifier (example: "--debug"). NULL value means no checking.
 * @param bool* container
 *  Pointer to the target boolean.
 */
int eargs_addArgumentFlag(char* argShort, char* argLong, bool* container);

/**
 * Adds an char* argument to the parsing list. The value will be assigned and must not be freed.
 * @param char* argShort
 *  Short version for the identifier (example: "-d"). NULL value means no checking.
 * @param char* argLong
 *  Long version of the identifier (example: "--debug"). NULL value means no checking.
 * @param char** container
 *  Pointer to a char pointer.
 */
int eargs_addArgumentString(char* argShort, char* argLong, char** container);

/*
 * This method will parse the argument list and writes all arguments in output
 * which are not an identifier or an argument for an identifier.
 *
 * @param int argc
 * 	Size of argv array.
 * @param char** argv
 * 	Array with arguments.
 *
 * @param char** output
 * 	Array for the output arguments. Array must be initialized and
 * 	have at least enough memory for containing all arguments from
 * 	argv or NULL.
 * @param void* config
 * 	Pointer of the config struct. This struct is passed to the
 * 	functions argument functions. It must be defined by the user of
 * 	this code
 *
 * @return int
 * 	Returns the number of strings in the output array.
 *
 */
int eargs_parse(int argc, char** argv, char** output, void* config);

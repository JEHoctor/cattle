/* program -- Tests related to program loading
 * Copyright (C) 2009-2010  Andrea Bolognani <eof@kiyuko.org>
 * This file is part of Cattle
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Homepage: http://kiyuko.org/software/cattle
 */

#include <glib.h>
#include <glib-object.h>
#include <cattle/cattle.h>

static void
program_create (CattleProgram **program,
                gconstpointer   data)
{
	*program = cattle_program_new ();
}

static void
program_destroy (CattleProgram **program,
                 gconstpointer   data)
{
	g_object_unref (*program);
}

/**
 * test_program_load_unbalanced_brackets:
 *
 * Make sure a program containing unbalanced brackets is not loaded,
 * and that the correct error is reported.
 */
static void
test_program_load_unbalanced_brackets (CattleProgram **program,
                                       gconstpointer   data)
{
	CattleInstruction *instruction;
	CattleInstructionValue value;
	GError *error = NULL;
	gboolean success;

	success = cattle_program_load (*program, "[", &error);

	g_assert (!success);
	g_assert (error != NULL);
	g_assert (error->domain == CATTLE_PROGRAM_ERROR);
	g_assert (error->code == CATTLE_PROGRAM_ERROR_UNBALANCED_BRACKETS);

	instruction = cattle_program_get_instructions (*program);

	g_assert (CATTLE_IS_INSTRUCTION (instruction));
	g_assert (cattle_instruction_get_next (instruction) == NULL);
	g_assert (cattle_instruction_get_loop (instruction) == NULL);

	value = cattle_instruction_get_value (instruction);

	g_assert (value == CATTLE_INSTRUCTION_NONE);

	g_object_unref (instruction);
}

/**
 * test_program_load_empty:
 *
 * Make sure an empty program can be loaded, and that the resulting
 * program consists of a single instruction with value
 * CATTLE_INSTRUCTION_NONE.
 */
static void
test_program_load_empty (CattleProgram **program,
                         gconstpointer   data)
{
	CattleInstruction *instruction;
	CattleInstructionValue value;
	GError *error = NULL;
	gboolean success;

	success = cattle_program_load (*program, "", &error);

	g_assert (success);
	g_assert (error == NULL);

	instruction = cattle_program_get_instructions (*program);

	g_assert (CATTLE_IS_INSTRUCTION (instruction));
	g_assert (cattle_instruction_get_next (instruction) == NULL);
	g_assert (cattle_instruction_get_loop (instruction) == NULL);

	value = cattle_instruction_get_value (instruction);

	g_assert (value == CATTLE_INSTRUCTION_NONE);

	g_object_unref (instruction);
}

/**
 * test_program_load_without_input:
 *
 * Load a program with no input.
 */
static void
test_program_load_without_input (CattleProgram **program,
                                 gconstpointer   data)
{
	CattleInstruction *instructions;
	CattleInstructionValue value;
	GError *error;
	gchar *input;
	gint quantity;
	gboolean success;

	error = NULL;
	success = cattle_program_load (*program,
	                               "+++>-<[-]",
	                               &error);

	g_assert (success);
	g_assert (error == NULL);

	instructions = cattle_program_get_instructions (*program);
	input = cattle_program_get_input (*program);

	g_assert (instructions != NULL);
	g_assert (input == NULL);

	value = cattle_instruction_get_value (instructions);
	quantity = cattle_instruction_get_quantity (instructions);

	g_assert (value == CATTLE_INSTRUCTION_INCREASE);
	g_assert (quantity == 3);

	g_object_unref (instructions);
}

/**
 * test_program_load_with_input:
 *
 * Load a program which containst some input along with the code.
 */
static void
test_program_load_with_input (CattleProgram **program,
                              gconstpointer   data)
{
	CattleInstruction *instructions;
	CattleInstructionValue value;
	GError *error;
	gchar *input;
	gboolean success;

	error = NULL;
	success = cattle_program_load (*program,
	                               ",[+.,]!some input",
	                               &error);

	g_assert (success);
	g_assert (error == NULL);

	instructions = cattle_program_get_instructions (*program);
	input = cattle_program_get_input (*program);

	g_assert (instructions != NULL);
	g_assert (input != NULL);
	g_assert (g_utf8_collate (input, "some input") == 0);

	value = cattle_instruction_get_value (instructions);

	g_assert (value == CATTLE_INSTRUCTION_READ);

	g_free (input);
	g_object_unref (instructions);
}

/**
 * test_program_load_double_loop:
 *
 * Load a program that is nothing but two loops nested.
 */
static void
test_program_load_double_loop (CattleProgram **program,
                               gconstpointer   data)
{
	CattleInstruction *current;
	CattleInstruction *outer_loop;
	CattleInstruction *inner_loop;
	CattleInstruction *next;
	CattleInstructionValue value;
	GError *error;
	gint quantity;
	gboolean success;

	error = NULL;
	success = cattle_program_load (*program, "[[]]", &error);

	g_assert (success);
	g_assert (error == NULL);

	/* First instruction: [ */
	outer_loop = cattle_program_get_instructions (*program);
	current = outer_loop;

	g_assert (current != NULL);

	value = cattle_instruction_get_value (current);
	quantity = cattle_instruction_get_quantity (current);

	g_assert (value == CATTLE_INSTRUCTION_LOOP_BEGIN);
	g_assert (quantity == 1);

	/* Enter the outer loop: [ */
	inner_loop = cattle_instruction_get_loop (current);
	current = inner_loop;

	g_assert (current != NULL);

	value = cattle_instruction_get_value (current);
	quantity = cattle_instruction_get_quantity (current);

	g_assert (value == CATTLE_INSTRUCTION_LOOP_BEGIN);
	g_assert (quantity == 1);

	/* Enter the inner loop: ] */
	next = cattle_instruction_get_loop (current);
	current = next;

	g_assert (current != NULL);

	value = cattle_instruction_get_value (current);
	quantity = cattle_instruction_get_quantity (current);

	g_assert (value == CATTLE_INSTRUCTION_LOOP_END);
	g_assert (quantity == 1);

	/* Inner loop is over */
	next = cattle_instruction_get_next (current);
	current = next;

	g_assert (current == NULL);

	/* After the inner loop: ] */
	next = cattle_instruction_get_next (inner_loop);
	g_object_unref (inner_loop);
	current = next;

	g_assert (current != NULL);

	value = cattle_instruction_get_value (current);
	quantity = cattle_instruction_get_quantity (current);

	g_assert (value == CATTLE_INSTRUCTION_LOOP_END);
	g_assert (quantity == 1);

	/* Outer loop is over */
	next = cattle_instruction_get_next (current);
	g_object_unref (current);
	current = next;

	g_assert (current == NULL);

	/* After the outer loop */
	next = cattle_instruction_get_next (outer_loop);
	g_object_unref (outer_loop);
	current = next;

	g_assert (current == NULL);
}

gint
main (gint argc, gchar **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/*
	g_test_add ("/program/load-unbalanced-brackets",
	            CattleProgram*,
	            NULL,
	            program_create,
	            test_program_load_unbalanced_brackets,
	            program_destroy);
	g_test_add ("/program/load-empty",
	            CattleProgram*,
	            NULL,
	            program_create,
	            test_program_load_empty,
	            program_destroy);
	g_test_add ("/program/load-without-input",
	            CattleProgram*,
	            NULL,
	            program_create,
	            test_program_load_without_input,
	            program_destroy);
	g_test_add ("/program/load-with-input",
	            CattleProgram*,
	            NULL,
	            program_create,
	            test_program_load_with_input,
	            program_destroy);
				*/
	g_test_add ("/program/load-double-loop",
	            CattleProgram*,
	            NULL,
	            program_create,
	            test_program_load_double_loop,
	            program_destroy);

	return g_test_run ();
}
/* Cattle -- Flexible Brainfuck interpreter library
 * Copyright (C) 2008-2010  Andrea Bolognani <eof@kiyuko.org>
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

#include "cattle-error.h"

/**
 * SECTION:cattle-error
 * @short_description: Load time and runtime errors
 *
 * Cattle uses the facilities provided by GLib for error reporting.
 *
 * Functions that can fail take a #GError as last argument; errors raised
 * are in the #CATTLE_ERROR domain with error codes from the #CattleError
 * enumeration.
 */

/**
 * CattleError:
 * @CATTLE_ERROR_BAD_UTF8: the provided input is not valid UTF-8.
 * @CATTLE_ERROR_IO: generic I/O error.
 * @CATTLE_ERROR_UNBALANCED_BRACKETS: the number of open and
 * closed brackets don't match.
 *
 * Errors detected either on code loading or at runtime.
 *
 * Cattle only supports UTF-8, so any input not using this encoding
 * causes an error to be raised.
 */

/**
 * CATTLE_ERROR:
 *
 * Error domain for Cattle. Errors in this domain will be from the
 * #CattleError enumeration.
 */
GQuark
cattle_error_quark (void)
{
	return g_quark_from_static_string ("cattle-error-quark");
}


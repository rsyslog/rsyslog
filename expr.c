/* expr.c - an expression class.
 * This module contains all code needed to represent expressions. Most
 * importantly, that means code to parse and execute them. Expressions
 * heavily depend on (loadable) functions, so it works in conjunction
 * with the function manager.
 *
 * Module begun 2007-11-30 by Rainer Gerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

/* This is the syntax of an expression. I keep it as inline documentation
 * as this enhances the chance that it is updates should there be a change.
 *
 * expr = (simple-string / template-string / function / property) [* expr ]
 * simple-string = "'" chars "'"
 * template-string =  '"' template-as--1.19.11-and-below '"'
 *                   ; string as used in previous $template directive
 * function = function-name "(" expr ")"
 * property = [list of property names]
 */

/* vi:set ai:
 */

/* shell.h
 *
 *  Copyright (c) 2017 Rodolfo García Peñas <kix@kix.es>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _WM_SHELL_H_
#define _WM_SHELL_H_

#include "wconfig.h"

void ExecuteShellCommand(virtual_screen *vscr, const char *command);
int execute_command(virtual_screen *vscr, char **argv, int argc);
int execute_command2(virtual_screen *vscr, char **argv, int argc);

#endif

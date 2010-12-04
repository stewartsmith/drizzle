/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/* Do not remove this file even though it is empty.
This file is included in univ.i and will cause compilation failure
if not present.
A custom checks have been added in the generated
storage/innobase/Makefile.in that is shipped with the InnoDB Plugin
source archive. These checks eventually define some macros and put
them in this file.
This is a hack that has been developed in order to deploy new compile
time checks without the need to regenerate the ./configure script that is
distributed in the MySQL 5.1 official source archives.
If by any chance Makefile.in and ./configure are regenerated and thus
the hack from Makefile.in wiped away then the "real" checks from plug.in
will take over.
*/

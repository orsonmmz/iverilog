/*
 * Copyright (c) 2013 Stephen Williams (steve@icarus.com)
 * Copyright CERN 2013 / Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

# include  "package.h"
# include  "subprogram.h"
# include  "entity.h"
# include  <iostream>
# include  "ivl_assert.h"

using namespace std;

int Package::emit_package(ostream&fd) const
{
	// Don't emit the package if there is nothing in it that SV
	// cares about.
      if (cur_types_.empty() && cur_constants_.empty() && cur_subprograms_.empty())
	    return 0;

	// If this package was imported from a library, then do not
	// emit it again.
      if (from_library_.str() != 0) {
	    fd << "/* Suppress package " << name()
	       << " from library " << from_library_ << " */" << endl;
	    return 0;
      }

      fd << "package \\" << name() << " ;" << endl;

      int errors = emit_global_types(fd);

	// Only emit types that were defined within this package. Skip
	// the types that were imported from elsewhere.
      for (map<perm_string,const VType*>::const_iterator cur = cur_types_.begin()
		 ; cur != cur_types_.end() ; ++ cur) {
	    fd << "typedef ";
	    errors += cur->second->emit_def(fd, cur->first);
	    fd << " ;" << endl;
      }

      //for (map<perm_string,struct const_t*>::const_iterator cur = use_constants_.begin()
		 //; cur != use_constants_.end() ; ++cur) {
	    //fd << "localparam \\" << cur->first << " = ";
	    //errors += cur->second->val->emit_package(fd);
	    //fd << ";" << endl;
      //}
      //for (map<perm_string,struct const_t*>::const_iterator cur = cur_constants_.begin()
		 //; cur != cur_constants_.end() ; ++cur) {
	    //fd << "localparam " << cur->first << " = ";
	    //errors += cur->second->val->emit_package(fd);
	    //fd << ";" << endl;
      //}

      for (map<perm_string,Subprogram*>::const_iterator cur = cur_subprograms_.begin()
		 ; cur != cur_subprograms_.end() ; ++ cur) {
	    errors += cur->second->emit_package(fd);
      }

      fd << "endpackage" << endl;

      return errors;
}

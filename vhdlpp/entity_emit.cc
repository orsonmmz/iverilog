/*
 * Copyright (c) 2011 Stephen Williams (steve@icarus.com)
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

# include  "entity.h"
# include  "architec.h"
# include  <iostream>
# include  <fstream>
# include  <iomanip>
# include  <ivl_assert.h>

int emit_entities(void)
{
      int errors = 0;

      for (map<perm_string,Entity*>::iterator cur = design_entities.begin()
		 ; cur != design_entities.end() ; ++cur) {
	    errors += cur->second->emit(cout);
      }

      return errors;
}

int Entity::emit(ostream&out)
{
      int errors = 0;

      out << "module \\" << get_name() << " ";

	// If there are generics, emit them
      if (parms_.size() > 0) {
	    out << "#(";
	    for (vector<InterfacePort*>::const_iterator cur = parms_.begin()
		       ; cur != parms_.end() ; ++cur) {
		  const InterfacePort*curp = *cur;
		  if (cur != parms_.begin())
			out << ", ";
		  out << "parameter \\" << curp->name << " = ";
		  ivl_assert(*this, curp->expr);
		  errors += curp->expr->emit(out, this, 0);
	    }
	    out << ") ";
      }

	// If there are ports, emit them.
      if (ports_.size() > 0) {
	    out << "(";
	    const char*sep = 0;
	    for (vector<InterfacePort*>::const_iterator cur = ports_.begin()
		       ; cur != ports_.end() ; ++cur) {
		  InterfacePort*port = *cur;

		  VType::decl_t&decl = declarations_[port->name];

		  if (sep) out << sep << endl;
		  else sep = ", ";

		  switch (port->mode) {
		      case PORT_NONE: // Should not happen
			out << "NO_PORT " << port->name;
			break;
		      case PORT_IN:
			out << "input ";
			errors += decl.emit(out, port->name);
			break;
		      case PORT_OUT:
			out << "output ";
			errors += decl.emit(out, port->name);
			break;
		  }
	    }
	    cout << ")";
      }

      out << ";" << endl;

      errors += emit_global_types(out);
      errors += bind_arch_->emit(out, this);

      out << "endmodule" << endl;

      return errors;
}

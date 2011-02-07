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
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  "entity.h"
# include  "architec.h"
# include  <iostream>
# include  <fstream>
# include  <iomanip>

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

      out << "module " << name_;

	// If there are ports, emit them.
      if (ports_.size() > 0) {
	    out << "(";
	    const char*sep = 0;
	    for (vector<InterfacePort*>::iterator cur = ports_.begin()
		       ; cur != ports_.end() ; ++cur) {
		  InterfacePort*port = *cur;

		  decl_t&decl = declarations_[port->name];

		  if (sep) out << sep;
		  else sep = ", ";

		  switch (port->mode) {
		      case PORT_NONE: // Should not happen
			out << "NO_PORT " << port->name;
			break;
		      case PORT_IN:
			out << "input ";
			if (decl.msb != decl.lsb)
			      out << "[" << decl.msb
				  << ":" << decl.lsb << "] ";
			out << port->name;
			break;
		      case PORT_OUT:
			out << "output ";
			if (decl.msb != decl.lsb)
			      out << "[" << decl.msb
				  << ":" << decl.lsb << "] ";
			out << port->name;
			break;
		  }
	    }
	    cout << ")";
      }

      out << ";" << endl;

      for (map<perm_string,decl_t>::const_iterator cur = declarations_.begin()
		 ; cur != declarations_.end() ; ++cur) {

	    switch (cur->second.type) {
		case VNONE:
		  out << "// N type for " << cur->first << endl;
		  break;
		case VLOGIC:
		  out << "wire logic ";
		  if (cur->second.msb != cur->second.lsb)
			out << "[" << cur->second.msb
			    << ":" << cur->second.lsb << "] ";
		  out << cur->first << ";" << endl;
		case VBOOL:
		  out << "wire bool ";
		  if (cur->second.signed_flag)
			out << "signed ";
		  if (cur->second.msb != cur->second.lsb)
			out << "[" << cur->second.msb
			    << ":" << cur->second.lsb << "] ";
		  out << cur->first << ";" << endl;
		  break;
	    }
      }

      errors += bind_arch_->emit(out, this);

      out << "endmodule" << endl;

      return errors;
}
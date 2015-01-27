/*
 * Copyright (c) 2011-2013 Stephen Williams (steve@icarus.com)
 * Copyright CERN 2012-2013 / Stephen Williams (steve@icarus.com)
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
 *    Picture Elements, Inc., 777 Panoramic Way, Berkeley, CA 94704.
 */

# include  "expression.h"
# include  "architec.h"
# include  "entity.h"
# include  "vsignal.h"
# include  "subprogram.h"
# include  "library.h"
# include  <iostream>
# include  <typeinfo>
# include  "parse_types.h"
# include  "compiler.h"
# include  "ivl_assert.h"

using namespace std;

int Expression::elaborate_lval(Entity*, Architecture*, bool)
{
      cerr << get_fileline() << ": error: Expression is not a valid l-value." << endl;
      return 1;
}

const VType* Expression::probe_type(Entity*, Architecture*) const
{
      return 0;
}

const VType* Expression::fit_type(Entity*ent, Architecture*arc, const VTypeArray*) const
{
      const VType*res = probe_type(ent,arc);
      if (res == 0) {
	    cerr << get_fileline() << ": internal error: "
		 << "fit_type for " << typeid(*this).name()
		 << " is not implemented." << endl;
      }

      return res;
}

const VType*ExpName::elaborate_adjust_type_with_range_(Entity*, Architecture*arc, const VType*type)
{
	// Unfold typedefs
      while (const VTypeDef*tdef = dynamic_cast<const VTypeDef*>(type)) {
	    type = tdef->peek_definition();
      }

      if (const VTypeArray*array = dynamic_cast<const VTypeArray*>(type)) {
	    if (index_ && !lsb_) {
		    // If the name is an array or a vector, then an
		    // indexed name has the type of the element.
		  type = array->element_type();

	    } else if (index_ && lsb_) {
		    // If the name is an array, then a part select is
		    // also an array, but with different bounds.
		  int64_t use_msb, use_lsb;
		  bool flag;

		  flag = index_->evaluate(arc, use_msb);
		  ivl_assert(*this, flag);
		  flag = lsb_->evaluate(arc, use_lsb);
		  ivl_assert(*this, flag);

		  Expression*exp_msb = new ExpInteger(use_msb);
		  Expression*exp_lsb = new ExpInteger(use_lsb);
		  vector<VTypeArray::range_t> use_dims (1);
		  use_dims[0] = VTypeArray::range_t(exp_msb, exp_lsb);
		  type = new VTypeArray(array->element_type(), use_dims);
	    }
      }

      return type;
}

int ExpName::elaborate_lval_(Entity*ent, Architecture*arc, bool is_sequ, ExpName*suffix)
{
      int errors = 0;

      if (debug_elaboration) {
	    debug_log_file << get_fileline() << ": ExpName::elaborate_lval_: "
			   << "name_=" << name_
			   << ", suffix->name()=" << suffix->name();
	    if (index_)
		  debug_log_file << ", index_=" << *index_;
	    if (lsb_)
		  debug_log_file << ", lsb_=" << *lsb_;
	    debug_log_file << endl;
      }

      if (prefix_.get()) {
	    cerr << get_fileline() << ": sorry: I don't know how to elaborate "
		 << "ExpName prefix of " << name_
		 << " in l-value expressions." << endl;
	    errors += 1;
      }

      const VType*found_type = 0;

      if (const InterfacePort*cur = ent->find_port(name_)) {
	    if (cur->mode != PORT_OUT) {
		  cerr << get_fileline() << ": error: Assignment to "
			"input port " << name_ << "." << endl;
		  return errors + 1;
	    }

	    if (is_sequ)
		  ent->set_declaration_l_value(name_, is_sequ);

	    found_type = cur->type;

      } else if (ent->find_generic(name_)) {

	    cerr << get_fileline() << ": error: Assignment to generic "
		 << name_ << " from entity "
		 << ent->get_name() << "." << endl;
	    return errors + 1;

      } else if (Signal*sig = arc->find_signal(name_)) {
	      // Tell the target signal that this may be a sequential l-value.
	    if (is_sequ) sig->count_ref_sequ();

	    found_type = sig->peek_type();

      } else if (Variable*var = arc->find_variable(name_)) {
	      // Tell the target signal that this may be a sequential l-value.
	    if (is_sequ) var->count_ref_sequ();

	    found_type = var->peek_type();
      }

	// Resolve type definition to get an actual type.
      while (const VTypeDef*tdef = dynamic_cast<const VTypeDef*> (found_type)) {
	    found_type = tdef->peek_definition();

	    if (debug_elaboration) {
		  debug_log_file << get_fileline() << ": ExpName::elaborate_lval_: "
				 << "Resolve typedef " << tdef->peek_name()
				 << " to defined type=" << typeid(*found_type).name()
				 << endl;
	    }
      }

      ivl_assert(*this, found_type);

	// If the prefix type is an array, then we may actually have a
	// case of an array of structs. For example:
	//   foo(n).bar
	// where foo is an array, (n) is an array index and foo(n) is
	// something that takes a suffix. For the purpose of our
	// expression type calculations, we need the element type.
      if (const VTypeArray*array = dynamic_cast<const VTypeArray*> (found_type)) {
	    found_type = array->element_type();

	    while (const VTypeDef*tdef = dynamic_cast<const VTypeDef*> (found_type)) {
		  found_type = tdef->peek_definition();
	    }

	    if (debug_elaboration) {
		  debug_log_file << get_fileline() << ": ExpName::elaborate_lval_: "
				 << "Extract array element type=" << typeid(*found_type).name()
				 << endl;
	    }
      }

      const VType*suffix_type = 0;

      if (const VTypeRecord*record = dynamic_cast<const VTypeRecord*> (found_type)) {
	    const VTypeRecord::element_t*element = record->element_by_name(suffix->name_);
	    ivl_assert(*this, element);

	    const VType*element_type = element->peek_type();
	    ivl_assert(*this, element_type);

	    suffix_type = element_type;

      }

      if (suffix_type == 0) {
	    cerr << get_fileline() << ": error: I don't know how to handle prefix " << name_
		 << " with suffix " << suffix->name_ << endl;
	    errors += 1;
	    return errors;
      }

      suffix_type = suffix->elaborate_adjust_type_with_range_(ent, arc, suffix_type);

      ivl_assert(*this, suffix_type);
      suffix->set_type(suffix_type);

      return errors;
}

int ExpName::elaborate_lval(Entity*ent, Architecture*arc, bool is_sequ)
{
      int errors = 0;

      if (prefix_.get()) {
	    return prefix_->elaborate_lval_(ent, arc, is_sequ, this);
      }

      const VType*found_type = 0;

      if (const InterfacePort*cur = ent->find_port(name_)) {
	    if (cur->mode != PORT_OUT) {
		  cerr << get_fileline() << ": error: Assignment to "
			"input port " << name_ << "." << endl;
		  return errors += 1;
	    }

	    if (is_sequ)
		  ent->set_declaration_l_value(name_, is_sequ);

	    found_type = cur->type;

      } else if (ent->find_generic(name_)) {

	    cerr << get_fileline() << ": error: Assignment to generic "
		 << name_ << " from entity "
		 << ent->get_name() << "." << endl;
	    return 1;

      } else if (Signal*sig = arc->find_signal(name_)) {
	      // Tell the target signal that this may be a sequential l-value.
	    if (is_sequ) sig->count_ref_sequ();

	    found_type = sig->peek_type();

      } else if (Variable*var = arc->find_variable(name_)) {
	      // Tell the target signal that this may be a sequential l-value.
	    if (is_sequ) var->count_ref_sequ();

	    found_type = var->peek_type();
      }

      if (found_type == 0) {
	    cerr << get_fileline() << ": error: Signal/variable " << name_
		 << " not found in this context." << endl;
	    return errors + 1;
      }

      found_type = elaborate_adjust_type_with_range_(ent, arc, found_type);

      set_type(found_type);
      return errors;
}

int ExpName::elaborate_rval(Entity*ent, Architecture*arc, const InterfacePort*lval)
{
      int errors = 0;

      if (prefix_.get()) {
	    cerr << get_fileline() << ": sorry: I don't know how to elaborate "
		 << "ExpName prefix parts in r-value expressions." << endl;
	    errors += 1;
      }

      if (const InterfacePort*cur = ent->find_port(name_)) {
        /* IEEE 1076-2008, p.80:
        * For a formal port IN, associated port should be IN, OUT, INOUT or BUFFER
        * For a formal port OUT, associated port should be OUT, INOUT or BUFFER
        * For a formal port INOUT, associated port should be OUT, INOUT or BUFFER
        * For a formal port BUFFER, associated port should be OUT, INOUT or BUFFER
        */
        switch(lval->mode) {
              case PORT_OUT:
              //case PORT_INOUT:
                  if (cur->mode == PORT_IN) {
                      cerr << get_fileline() << ": error: Connecting "
                      "formal output port " << lval->name << " to actual input port "
                      << name_ << "." << endl;
                      errors += 1;
                  }
                  break;
              case PORT_IN:
              case PORT_NONE:
              default:
                break;
        }
      } else if (arc->find_signal(name_)) {
	      /* OK */

      } else if (ent->find_generic(name_)) {
	      /* OK */

      } else {
            cerr << get_fileline() << ": error: No port or signal " << name_
		 << " to be used as r-value." << endl;
            errors += 1;
      }

      return errors;
}

int ExpNameALL::elaborate_lval(Entity*ent, Architecture*arc, bool is_sequ)
{
      return Expression::elaborate_lval(ent, arc, is_sequ);
}

int Expression::elaborate_expr(Entity*, Architecture*, const VType*)
{
      cerr << get_fileline() << ": internal error: I don't know how to elaborate expression type=" << typeid(*this).name() << endl;
      return 1;
}

const VType* ExpBinary::probe_type(Entity*ent, Architecture*arc) const
{
      const VType*t1 = operand1_->probe_type(ent, arc);
      const VType*t2 = operand2_->probe_type(ent, arc);

      if (t1 == 0)
	    return t2;
      if (t2 == 0)
	    return t1;

      if (t1 == t2)
	    return t1;

      if (const VType*tb = resolve_operand_types_(t1, t2))
	    return tb;

	// FIXME: I should at this point try harder to find an
	// operator that has the proper argument list and use this
	// here, but for now we leave it for the back-end to figure out.
#if 0
      cerr << get_fileline() << ": internal error: I don't know how to resolve types of generic binary expressions." << endl;
#endif
      return 0;
}

const VType*ExpBinary::resolve_operand_types_(const VType*, const VType*) const
{
      return 0;
}

int ExpBinary::elaborate_exprs(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      errors += operand1_->elaborate_expr(ent, arc, ltype);
      errors += operand2_->elaborate_expr(ent, arc, ltype);
      return errors;
}

/*
 * the default fit_type method for unary operator expressions is to
 * return the fit_type for the operand. The assumption is that the
 * operator doesn't change the type.
 */
const VType*ExpUnary::fit_type(Entity*ent, Architecture*arc, const VTypeArray*atype) const
{
      return operand1_->fit_type(ent, arc, atype);
}

const VType*ExpAggregate::probe_type(Entity*ent, Architecture*arc) const
{
      return Expression::probe_type(ent, arc);
}

const VType*ExpAggregate::fit_type(Entity*, Architecture*, const VTypeArray*host) const
{
      ivl_assert(*this, elements_.size() == 1);
      size_t choice_count = elements_[0]->count_choices();

      ivl_assert(*this, choice_count > 0);
      vector<choice_element> ce (choice_count);
      elements_[0]->map_choices(&ce[0]);

      ivl_assert(*this, ce.size() == 1);
      prange_t*prange = ce[0].choice->range_expressions();
      ivl_assert(*this, prange);

      Expression*use_msb = prange->msb();
      Expression*use_lsb = prange->lsb();

      ivl_assert(*this, host->dimensions() == 1);
      vector<VTypeArray::range_t> range (1);

      range[0] = VTypeArray::range_t(use_msb, use_lsb);

      const VTypeArray*res = new VTypeArray(host->element_type(), range);

      return res;
}

int ExpAggregate::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      if (ltype == 0) {
	    cerr << get_fileline() << ": error: Elaboration of aggregate types needs well known type context?" << endl;
	    return 1;
      }

      set_type(ltype);

      while (const VTypeDef*cur = dynamic_cast<const VTypeDef*>(ltype)) {
	    ltype = cur->peek_definition();
      }

      if (const VTypeArray*larray = dynamic_cast<const VTypeArray*>(ltype)) {
	    return elaborate_expr_array_(ent, arc, larray);
      }
      else if(const VTypeRecord*lrecord = dynamic_cast<const VTypeRecord*>(ltype)) {
            return elaborate_expr_record_(ent, arc, lrecord);
      }

      cerr << get_fileline() << ": internal error: I don't know how to elaborate aggregate expressions. type=" << typeid(*ltype).name() << endl;
      return 1;
}

/*
 * Elaboration of array aggregates is elaboration of the element
 * expressions (the elements_ member) using the element type as the
 * ltype for the subexpression.
 */
int ExpAggregate::elaborate_expr_array_(Entity*ent, Architecture*arc, const VTypeArray*ltype)
{
      const VType*element_type = ltype->element_type();
      int errors = 0;
      size_t choice_count = 0;

	// Figure out how many total elements we have here. Note that
	// each parsed element may be bound to multiple choices, so
	// account for that.
      for (size_t edx = 0 ; edx < elements_.size() ; edx += 1) {
	    element_t*ecur = elements_[edx];
	    if (ecur->count_choices() == 0)
		  choice_count += 1;
	    else
		  choice_count += ecur->count_choices();
      }

      aggregate_.resize(choice_count);

	// Translate the elements_ array to the aggregate_ array. In
	// the target array, each expression is attached to a single
	// choice.
      size_t cdx = 0;
      for (size_t edx = 0 ; edx < elements_.size() ; edx += 1) {
	    element_t*ecur = elements_[edx];
	    if (ecur->count_choices() == 0) {
		    // positional associations have no "choice"
		    // associated with them.
		  aggregate_[cdx].choice = 0;
		  aggregate_[cdx].expr = ecur->extract_expression();
		  aggregate_[cdx].alias_flag = false;
		  cdx += 1;
	    } else {
		  ecur->map_choices(&aggregate_[cdx]);
		  cdx += ecur->count_choices();
	    }
      }

      ivl_assert(*this, cdx == choice_count);

	// Now run through the more convenient mapping and elaborate
	// all the expressions that I find.
      for (size_t idx = 0 ; idx < aggregate_.size() ; idx += 1) {
	    if (aggregate_[idx].alias_flag)
		  continue;

	    errors += aggregate_[idx].expr->elaborate_expr(ent, arc, element_type);
      }

	// done with the obsolete elements_ vector.
      elements_.clear();

      return errors;
}

int ExpAggregate::elaborate_expr_record_(Entity*ent, Architecture*arc, const VTypeRecord*ltype)
{
      int errors = 0;

      aggregate_.resize(elements_.size());
      choice_element tmp;
      int idx;

	// Translate the elements_ array to the aggregate_ array. In
	// the target array, each expression is attached to a single
	// choice.
      for (size_t edx = 0 ; edx < elements_.size() ; edx += 1) {
            element_t*ecur = elements_[edx];

            // it is invalid to have more than one choice in record assignment
            ivl_assert(*this, ecur->count_choices() == 1);

            ecur->map_choices(&tmp);
            choice_t*ch = tmp.choice;

            ivl_assert(*this, !ch->others());
            ivl_assert(*this, !tmp.alias_flag);

	// Get the appropriate type for a field
            const ExpName*field = dynamic_cast<const ExpName*>(ch->simple_expression(false));
            ivl_assert(*this, field);

            perm_string field_name = field->peek_name();
            const VTypeRecord::element_t*el = ltype->element_by_name(field_name, &idx);

            aggregate_[idx] = tmp;
            errors += aggregate_[idx].expr->elaborate_expr(ent, arc, el->peek_type());
      }

	// done with the obsolete elements_ vector.
      elements_.clear();

      return errors;
}

void ExpAggregate::element_t::map_choices(ExpAggregate::choice_element*dst)
{
      for (size_t idx = 0 ; idx < fields_.size() ; idx += 1) {
	    dst->choice = fields_[idx];
	    dst->expr = val_;
	    dst->alias_flag = (idx != 0);
	    dst += 1;
      }
}

int ExpArithmetic::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0) {
	    ltype = probe_type(ent, arc);
      }

      ivl_assert(*this, ltype != 0);
      errors += elaborate_exprs(ent, arc, ltype);
      return errors;
}

const VType* ExpArithmetic::resolve_operand_types_(const VType*t1, const VType*t2) const
{
      while (const VTypeRange*tmp = dynamic_cast<const VTypeRange*> (t1))
	    t1 = tmp->base_type();
      while (const VTypeRange*tmp = dynamic_cast<const VTypeRange*> (t2))
	    t2 = tmp->base_type();

      if (t1->type_match(t2))
	    return t1;
      if (t2->type_match(t2))
	    return t2;

      return 0;
}

const VType* ExpAttribute::probe_type(Entity*ent, Architecture*arc) const
{
      base_->probe_type(ent, arc);

      if (name_ == "length" || name_ == "left" || name_ == "right") {
	    return &primitive_INTEGER;
      }

      return 0;
}

int ExpAttribute::elaborate_expr(Entity*ent, Architecture*arc, const VType*)
{
      int errors = 0;
      const VType*sub_type = base_->probe_type(ent, arc);
      errors += base_->elaborate_expr(ent, arc, sub_type);
      return errors;
}

const VType*ExpBitstring::fit_type(Entity*, Architecture*, const VTypeArray*atype) const
{
	// Really should check that this string can work with the
	// array element type?
      return atype->element_type();
}

int ExpBitstring::elaborate_expr(Entity*, Architecture*, const VType*)
{
      int errors = 0;
      return errors;
}

const VType*ExpCharacter::fit_type(Entity*, Architecture*, const VTypeArray*atype) const
{
	// Really should check that this character can work with the
	// array element type?
      return atype->element_type();
}

int ExpCharacter::elaborate_expr(Entity*, Architecture*, const VType*ltype)
{
      ivl_assert(*this, ltype != 0);
      set_type(ltype);
      return 0;
}

const VType*ExpConcat::fit_type(Entity*ent, Architecture*arc, const VTypeArray*atype) const
{
      Expression*operands[2] = {operand1_, operand2_};
      const VType*types[2] = {NULL, NULL};
      Expression*sizes[2] = {NULL, NULL};

      // determine the type and size of concatenated expressions
      for(int i = 0; i < 2; ++i) {
	    types[i] = operands[i]->fit_type(ent, arc, atype);

	    if(const VTypeArray*arr = dynamic_cast<const VTypeArray*>(types[i])) {
		types[i] = arr->element_type();
		ivl_assert(*this, arr->dimensions() == 1);
		const VTypeArray::range_t&dim = arr->dimension(0);
		sizes[i] = new ExpArithmetic(ExpArithmetic::MINUS, dim.msb(), dim.lsb());
	    } else {
		sizes[i] = new ExpInteger(0);
	    }
      }

      // the range of the concatenated expression is (size1 + size2 + 1):0
      // note that each of the sizes are already decreased by one,
      // e.g. 3:0 <=> size == 3 even though there are 4 bits
      Expression*size = new ExpArithmetic(ExpArithmetic::PLUS,
                            new ExpArithmetic(ExpArithmetic::PLUS, sizes[0], sizes[1]),
                            new ExpInteger(1));

      std::list<prange_t*> ranges;
      ranges.push_front(new prange_t(size, new ExpInteger(0), true));
      const VType*array = new VTypeArray(types[1], &ranges);

      return array;
}
/*
 * I don't know how to probe the type of a concatenation, quite yet.
 */
const VType*ExpConcat::probe_type(Entity*, Architecture*) const
{
      ivl_assert(*this, 0);
      return 0;
}

int ExpConcat::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0) {
	    ltype = probe_type(ent, arc);
      }

      ivl_assert(*this, ltype != 0);

      if (const VTypeArray*atype = dynamic_cast<const VTypeArray*>(ltype)) {
	    errors += elaborate_expr_array_(ent, arc, atype);
      } else {
	    errors += operand1_->elaborate_expr(ent, arc, ltype);
	    errors += operand2_->elaborate_expr(ent, arc, ltype);
      }

      return errors;
}

int ExpConcat::elaborate_expr_array_(Entity*ent, Architecture*arc, const VTypeArray*atype)
{
      int errors = 0;

	// For now, only support single-dimension arrays here.
      ivl_assert(*this, atype->dimensions() == 1);

      const VType*type1 = operand1_->fit_type(ent, arc, atype);
      ivl_assert(*this, type1);

      const VType*type2 = operand2_->fit_type(ent, arc, atype);
      ivl_assert(*this, type2);

      errors += operand1_->elaborate_expr(ent, arc, type1);
      errors += operand2_->elaborate_expr(ent, arc, type2);

      return errors;
}

const VType* ExpConditional::probe_type(Entity*, Architecture*) const
{
      return 0;
}

int ExpConditional::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0)
	    ltype = probe_type(ent, arc);

      ivl_assert(*this, ltype);

      set_type(ltype);

	/* Note that the type for the condition expression need not
	   have anything to do with the type of this expression. */
      errors += cond_->elaborate_expr(ent, arc, 0);

      for (list<Expression*>::const_iterator cur = true_clause_.begin()
		 ; cur != true_clause_.end() ; ++cur) {
	    errors += (*cur)->elaborate_expr(ent, arc, ltype);
      }

      for (list<else_t*>::const_iterator cur = else_clause_.begin()
		 ; cur != else_clause_.end() ; ++cur) {
	    errors += (*cur)->elaborate_expr(ent, arc, ltype);
      }

      return errors;
}

int ExpConditional::else_t::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (cond_)
	    errors += cond_->elaborate_expr(ent, arc, 0);

      for (list<Expression*>::const_iterator cur = true_clause_.begin()
		 ; cur != true_clause_.end() ; ++cur) {
	    errors += (*cur)->elaborate_expr(ent, arc, ltype);
      }

      return errors;
}

int ExpFunc::elaborate_expr(Entity*ent, Architecture*arc, const VType*)
{
      int errors = 0;

      ivl_assert(*this, arc);
      Subprogram*prog = arc->find_subprogram(name_);

      if(!prog)
            prog = library_find_subprogram(name_);

      ivl_assert(*this, def_==0);
      def_ = prog;

	// Elaborate arguments
      for (size_t idx = 0 ; idx < argv_.size() ; idx += 1) {
	    const VType*tmp = argv_[idx]->probe_type(ent, arc);
	    const VType*param_type = prog ? prog->peek_param_type(idx) : NULL;

	    if(!tmp && param_type)
	        tmp = param_type;

	    errors += argv_[idx]->elaborate_expr(ent, arc, tmp);

            // Type casting for unbounded arrays
            if(param_type && param_type->is_unbounded() /*&& !param_type->type_match(tmp)*/) {
                argv_[idx] = new ExpCast(argv_[idx], get_global_typedef(param_type));
            }
      }

      return errors;
}

const VType* ExpInteger::probe_type(Entity*, Architecture*) const
{
      return &primitive_INTEGER;
}

int ExpInteger::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0) {
	    ltype = probe_type(ent, arc);
      }

      ivl_assert(*this, ltype != 0);

      return errors;
}

const VType* ExpReal::probe_type(Entity*, Architecture*) const
{
      return &primitive_REAL;
}

int ExpReal::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0) {
        ltype = probe_type(ent, arc);
      }

      ivl_assert(*this, ltype != 0);

      return errors;
}

int ExpLogical::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0) {
	    ltype = probe_type(ent, arc);
      }

      ivl_assert(*this, ltype != 0);
      errors += elaborate_exprs(ent, arc, ltype);
      return errors;
}

const VType* ExpName::probe_prefix_type_(Entity*ent, Architecture*arc) const
{
      if (prefix_.get()) {
	    cerr << get_fileline() << ": sorry: I do not know how to support nested prefix parts." << endl;
	    return 0;
      }

      const VType*type = probe_type(ent, arc);
      return type;
}

/*
 * This method is the probe_type() imlementation for ExpName objects
 * that have prefix parts. In this case we try to get the type of the
 * prefix and interpret the name in that context.
 */
const VType* ExpName::probe_prefixed_type_(Entity*ent, Architecture*arc) const
{
	// First, get the type of the prefix.
      const VType*prefix_type = prefix_->probe_prefix_type_(ent, arc);
      if (prefix_type == 0) {
	    return 0;
      }

      while (const VTypeDef*def = dynamic_cast<const VTypeDef*> (prefix_type)) {
	    prefix_type = def->peek_definition();
      }

	// If the prefix type is a record, then the current name is
	// the name of a member.
      if (const VTypeRecord*pref_record = dynamic_cast<const VTypeRecord*> (prefix_type)) {
	    const VTypeRecord::element_t*element = pref_record->element_by_name(name_);
	    ivl_assert(*this, element);

	    const VType*element_type = element->peek_type();
	    ivl_assert(*this, element_type);

	    return element_type;
      }

      cerr << get_fileline() << ": sorry: I don't know how to probe "
	   << "prefix type " << typeid(*prefix_type).name()
	   << " of " << name_ << "." << endl;

      return 0;
}

const VType* ExpName::probe_type(Entity*ent, Architecture*arc) const
{
      if (prefix_.get())
	    return probe_prefixed_type_(ent, arc);

      if (const InterfacePort*cur = ent->find_port(name_)) {
	    ivl_assert(*this, cur->type);
	    return cur->type;
      }

      if (const InterfacePort*cur = ent->find_generic(name_)) {
	    ivl_assert(*this, cur->type);
	    return cur->type;
      }

      if (Signal*sig = arc->find_signal(name_))
	    return sig->peek_type();

      if (Variable*var = arc->find_variable(name_))
	    return var->peek_type();

      const VType*ctype = 0;
      Expression*cval = 0;
      if (arc->find_constant(name_, ctype, cval))
	    return ctype;

      if (const VType*gtype = arc->probe_genvar_type(name_)) {
	    return gtype;
      }

      cerr << get_fileline() << ": error: Signal/variable " << name_
	   << " not found in this context." << endl;
      return 0;
}

const VType* ExpName::fit_type(Entity*ent, Architecture*arc, const VTypeArray*)const
{
      return probe_type(ent, arc);
}

int ExpName::elaborate_expr(Entity*, Architecture*, const VType*ltype)
{
      if (ltype) {
	    ivl_assert(*this, ltype != 0);
	    set_type(ltype);
      }

      return 0;
}

const VType* ExpNameALL::probe_type(Entity*, Architecture*) const
{
      return 0;
}

const VType* ExpRelation::probe_type(Entity*, Architecture*) const
{
      return &primitive_BOOLEAN;
}

int ExpRelation::elaborate_expr(Entity*ent, Architecture*arc, const VType*ltype)
{
      int errors = 0;

      if (ltype == 0) {
	    ltype = probe_type(ent, arc);
      }

      ivl_assert(*this, ltype != 0);

	// The type of the operands must match, but need not match the
	// type for the ExpRelation itself. So get the operand type
	// separately.
      const VType*otype = ExpBinary::probe_type(ent, arc);
      errors += elaborate_exprs(ent, arc, otype);

      return errors;
}

/*
 * When a string appears in a concatenation, then the type of the
 * string is an array with the same element type of the concatenation,
 * but with elements for each character of the string.
 */
const VType*ExpString::fit_type(Entity*, Architecture*, const VTypeArray*atype) const
{
      vector<VTypeArray::range_t> range (atype->dimensions());

	// Generate an array range for this string
      ivl_assert(*this, range.size() == 1);
      ExpInteger*use_msb = new ExpInteger(value_.size());
      ExpInteger*use_lsb = new ExpInteger(0);
      FILE_NAME(use_msb, this);
      FILE_NAME(use_lsb, this);
      range[0] = VTypeArray::range_t(use_msb, use_lsb);

      VTypeArray*type = new VTypeArray(atype->element_type(), range);
      return type;
}

int ExpString::elaborate_expr(Entity*, Architecture*, const VType*ltype)
{
      ivl_assert(*this, ltype != 0);
      set_type(ltype);
      return 0;
}

int ExpUNot::elaborate_expr(Entity*, Architecture*, const VType*ltype)
{
      ivl_assert(*this, ltype != 0);
      set_type(ltype);
      return 0;
}

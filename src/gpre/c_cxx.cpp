//____________________________________________________________
//  
//		PROGRAM:	C preprocess
//		MODULE:		c_cxx.cpp
//		DESCRIPTION:	C and C++ code generator
//  
//  The contents of this file are subject to the Interbase Public
//  License Version 1.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy
//  of the License at http://www.Inprise.com/IPL.html
//  
//  Software distributed under the License is distributed on an
//  "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
//  or implied. See the License for the specific language governing
//  rights and limitations under the License.
//  
//  The Original Code was created by Inprise Corporation
//  and its predecessors. Portions created by Inprise Corporation are
//  Copyright (C) Inprise Corporation.
//  
//  All Rights Reserved.
//  Contributor(s): ______________________________________.16/09/2003
//  TMN (Mike Nordell) 11.APR.2001 - Reduce compiler warnings
//  
// 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
//
//
//____________________________________________________________
//
//	$Id: c_cxx.cpp,v 1.39 2003-11-28 06:48:11 robocop Exp $
//

#include "firebird.h"
#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/y_ref.h"
#include "../jrd/ibase.h"
#include "../gpre/gpre.h"
#include "../gpre/pat.h"
#include "../gpre/msc_proto.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/lang_proto.h"
#include "../gpre/pat_proto.h"
#include "../gpre/prett_proto.h"
#include "../jrd/gds_proto.h"


extern TEXT* transaction_name;

static void align(int);
static void asgn_from(const act*, REF, int);
static void asgn_to(const act*, REF, int);
static void asgn_to_proc(const ref*, int);
static void gen_any(const act*, int);
static void gen_at_end(const act*, int);
static void gen_based(const act*, int);
static void gen_blob_close(const act*, USHORT);
static void gen_blob_end(const act*, USHORT);
static void gen_blob_for(const act*, USHORT);
static void gen_blob_open(const act*, USHORT);
static void gen_blr(void*, SSHORT, const char*);
static void gen_clear_handles(const act*, int);
static void gen_compatibility_symbol(const TEXT*, const TEXT*, const TEXT*);
static void gen_compile(const act*, int);
static void gen_create_database(const act*, int);
static int gen_cursor_close(const act*, const gpre_req*, int);
static void gen_cursor_init(const act*, int);
static int gen_cursor_open(const act*, const gpre_req*, int);
static void gen_database(const act*, int);
static void gen_ddl(const act*, int);
static void gen_drop_database(const act*, int);
static void gen_dyn_close(const act*, int);
static void gen_dyn_declare(const act*, int);
static void gen_dyn_describe(const act*, int, bool);
static void gen_dyn_execute(const act*, int);
static void gen_dyn_fetch(const act*, int);
static void gen_dyn_immediate(const act*, int);
static void gen_dyn_insert(const act*, int);
static void gen_dyn_open(const act*, int);
static void gen_dyn_prepare(const act*, int);
static void gen_emodify(const act*, int);
static void gen_estore(const act*, int);
static void gen_endfor(const act*, int);
static void gen_erase(const act*, int);
static SSHORT gen_event_block(const act*);
static void gen_event_init(const act*, int);
static void gen_event_wait(const act*, int);
static void gen_fetch(const act*, int);
static void gen_finish(const act*, int);
static void gen_for(const act*, int);
static void gen_function(const act*, int);
static void gen_get_or_put_slice(const act*, REF, bool, int);
static void gen_get_segment(const act*, int);
static void gen_loop(const act*, int);
static TEXT* gen_name(char*, const ref*, bool);
static void gen_on_error(const act*, USHORT);
static void gen_procedure(const act*, int);
static void gen_put_segment(const act*, int);
static void gen_raw(const UCHAR*, int);
static void gen_ready(const act*, int);
static void gen_receive(const act*, int, POR);
static void gen_release(const act*, int);
static void gen_request(const gpre_req*);
static void gen_return_value(const act*, int);
static void gen_routine(const act*, int);
static void gen_s_end(const act*, int);
static void gen_s_fetch(const act*, int);
static void gen_s_start(const act*, int);
static void gen_segment(const act*, int);
static void gen_select(const act*, int);
static void gen_send(const act*, POR, int);
static void gen_slice(const act*, REF, int);
static void gen_start(const act*, POR, int, bool);
static void gen_store(const act*, int);
static void gen_t_start(const act*, int);
static void gen_tpb(TPB, int);
static void gen_trans(const act*, int);
static void gen_type(const act*, int);
static void gen_update(const act*, int);
static void gen_variable(const act*, int);
static void gen_whenever(const swe*, int);
static void make_array_declaration(ref*);
static TEXT* make_name(TEXT*, sym*);
static void make_ok_test(const act*, const gpre_req*, int);
static void make_port(const por*, int);
static void make_ready(const dbb*, const TEXT*, const TEXT*, USHORT,
	const gpre_req*);
static void printa(int, const char*, ...) ATTRIBUTE_FORMAT(2,3);
static void printb(const TEXT*, ...) ATTRIBUTE_FORMAT(1,2);
static const TEXT* request_trans(const act*, const gpre_req*);
static const TEXT* status_vector(const act*);
static void t_start_auto(const act*, const gpre_req*, const TEXT*, int, bool);

static bool global_first_flag = false;
static TEXT* global_status_name;

const int INDENT	= 3;

static const char* const NULL_STRING	= "(char *)0";
static const char* const NULL_STATUS	= "(long*) 0L";
static const char* const NULL_SQLDA	= "(XSQLDA*) 0L";

#ifdef VMS
static const char* const GDS_INCLUDE	= "\"interbase:[syslib]ibase.h\"";
#elif DARWIN
static const char* const GDS_INCLUDE	= "<Firebird/ibase.h>";
#else
static const char* const GDS_INCLUDE	= "<ibase.h>";
#endif

static const char* const DCL_LONG	= "long";
static const char* const DCL_QUAD	= "ISC_QUAD";

static inline void begin(const int column)
{
	printa(column, "{");
}

static inline void endp(const int column)
{
	printa(column, "}");
}

static inline void set_sqlcode(const act* action, const int column)
{
	if (action->act_flags & ACT_sql)
		printa(column, "SQLCODE = isc_sqlcode(%s);", global_status_name);
}

//____________________________________________________________
//  
//  

void C_CXX_action(const act* action, int column)
{

	global_status_name = "isc_status";

//  Put leading braces where required 

	switch (action->act_type) {
	case ACT_alter_database:
	case ACT_alter_domain:
	case ACT_alter_index:
	case ACT_alter_table:
	case ACT_blob_close:
	case ACT_blob_create:
	case ACT_blob_for:
	case ACT_blob_open:
	case ACT_clear_handles:
	case ACT_close:
	case ACT_commit:
	case ACT_commit_retain_context:
	case ACT_create_database:
	case ACT_create_domain:
	case ACT_create_generator:
	case ACT_create_index:
	case ACT_create_shadow:
	case ACT_create_table:
	case ACT_create_view:
	case ACT_declare_filter:
	case ACT_declare_udf:
	case ACT_disconnect:
	case ACT_drop_database:
	case ACT_drop_domain:
	case ACT_drop_filter:
	case ACT_drop_index:
	case ACT_drop_shadow:
	case ACT_drop_table:
	case ACT_drop_udf:
	case ACT_drop_view:
	case ACT_dyn_close:
	case ACT_dyn_cursor:
	case ACT_dyn_describe:
	case ACT_dyn_describe_input:
	case ACT_dyn_execute:
	case ACT_dyn_fetch:
	case ACT_dyn_grant:
	case ACT_dyn_immediate:
	case ACT_dyn_insert:
	case ACT_dyn_open:
	case ACT_dyn_prepare:
	case ACT_dyn_revoke:
	case ACT_fetch:
	case ACT_finish:
	case ACT_for:
	case ACT_get_segment:
	case ACT_get_slice:
	case ACT_insert:
	case ACT_loop:
	case ACT_modify:
	case ACT_open:
	case ACT_prepare:
	case ACT_procedure:
	case ACT_put_slice:
	case ACT_ready:
	case ACT_release:
	case ACT_rfinish:
	case ACT_rollback:
	case ACT_s_fetch:
	case ACT_s_start:
	case ACT_select:
	case ACT_store:
	case ACT_start:
	case ACT_update:
	case ACT_statistics:
		begin(column);
	}

	switch (action->act_type) {
	case ACT_alter_database:
	case ACT_alter_domain:
	case ACT_alter_index:
	case ACT_alter_table:
		gen_ddl(action, column);
		break;
	case ACT_any:
		gen_any(action, column);
		return;
	case ACT_at_end:
		gen_at_end(action, column);
		return;
	case ACT_b_declare:
		gen_database(action, column);
		gen_routine(action, column);
		return;
	case ACT_basedon:
		gen_based(action, column);
		return;
	case ACT_blob_cancel:
		gen_blob_close(action, (USHORT) column);
		return;
	case ACT_blob_close:
		gen_blob_close(action, (USHORT) column);
		break;
	case ACT_blob_create:
		gen_blob_open(action, (USHORT) column);
		break;
	case ACT_blob_for:
		gen_blob_for(action, (USHORT) column);
		return;
	case ACT_blob_handle:
		gen_segment(action, column);
		return;
	case ACT_blob_open:
		gen_blob_open(action, (USHORT) column);
		break;
	case ACT_clear_handles:
		gen_clear_handles(action, column);
		break;
	case ACT_close:
		gen_s_end(action, column);
		break;
	case ACT_commit:
		gen_trans(action, column);
		break;
	case ACT_commit_retain_context:
		gen_trans(action, column);
		break;
	case ACT_create_database:
		gen_create_database(action, column);
		break;
	case ACT_create_domain:
	case ACT_create_generator:
	case ACT_create_index:
	case ACT_create_shadow:
	case ACT_create_table:
	case ACT_create_view:
		gen_ddl(action, column);
		break;
	case ACT_cursor:
		gen_cursor_init(action, column);
		return;
	case ACT_database:
		gen_database(action, column);
		return;
	case ACT_declare_filter:
	case ACT_declare_udf:
		gen_ddl(action, column);
		break;
	case ACT_disconnect:
		gen_finish(action, column);
		break;
	case ACT_drop_database:
		gen_drop_database(action, column);
		break;
	case ACT_drop_domain:
	case ACT_drop_filter:
	case ACT_drop_index:
	case ACT_drop_shadow:
	case ACT_drop_table:
	case ACT_drop_udf:
	case ACT_drop_view:
		gen_ddl(action, column);
		break;
	case ACT_dyn_close:
		gen_dyn_close(action, column);
		break;
	case ACT_dyn_cursor:
		gen_dyn_declare(action, column);
		break;
	case ACT_dyn_describe:
		gen_dyn_describe(action, column, false);
		break;
	case ACT_dyn_describe_input:
		gen_dyn_describe(action, column, true);
		break;
	case ACT_dyn_execute:
		gen_dyn_execute(action, column);
		break;
	case ACT_dyn_fetch:
		gen_dyn_fetch(action, column);
		break;
	case ACT_dyn_grant:
		gen_ddl(action, column);
		break;
	case ACT_dyn_immediate:
		gen_dyn_immediate(action, column);
		break;
	case ACT_dyn_insert:
		gen_dyn_insert(action, column);
		break;
	case ACT_dyn_open:
		gen_dyn_open(action, column);
		break;
	case ACT_dyn_prepare:
		gen_dyn_prepare(action, column);
		break;
	case ACT_dyn_revoke:
		gen_ddl(action, column);
		break;
	case ACT_endblob:
		gen_blob_end(action, (USHORT) column);
		return;
	case ACT_enderror:{
			column += INDENT;
			endp(column);
			column -= INDENT;
		}
		break;
	case ACT_endfor:
		gen_endfor(action, column);
		break;
	case ACT_endmodify:
		gen_emodify(action, column);
		break;
	case ACT_endstore:
		gen_estore(action, column);
		break;
	case ACT_erase:
		gen_erase(action, column);
		return;
	case ACT_event_init:
		gen_event_init(action, column);
		break;
	case ACT_event_wait:
		gen_event_wait(action, column);
		break;
	case ACT_fetch:
		gen_fetch(action, column);
		break;
	case ACT_finish:
		gen_finish(action, column);
		break;
	case ACT_for:
		gen_for(action, column);
		return;
	case ACT_function:
		gen_function(action, column);
		return;
	case ACT_get_segment:
		gen_get_segment(action, column);
		break;
	case ACT_get_slice:
		gen_slice(action, 0, column);
		break;
	case ACT_hctef:
		endp(column);
		break;
	case ACT_insert:
		gen_s_start(action, column);
		break;
	case ACT_loop:
		gen_loop(action, column);
		break;
	case ACT_on_error:
		gen_on_error(action, (USHORT) column);
		return;
	case ACT_open:
		gen_s_start(action, column);
		break;
	case ACT_prepare:
		gen_trans(action, column);
		break;
	case ACT_procedure:
		gen_procedure(action, column);
		break;
	case ACT_put_segment:
		gen_put_segment(action, column);
		break;
	case ACT_put_slice:
		gen_slice(action, 0, column);
		break;
	case ACT_ready:
		gen_ready(action, column);
		break;
	case ACT_release:
		gen_release(action, column);
		break;
	case ACT_rfinish:
		gen_finish(action, column);
		break;
	case ACT_rollback:
		gen_trans(action, column);
		break;
	case ACT_routine:
		gen_routine(action, column);
		return;
	case ACT_s_end:
		gen_s_end(action, column);
		return;
	case ACT_s_fetch:
		gen_s_fetch(action, column);
		return;
	case ACT_s_start:
		gen_s_start(action, column);
		break;
	case ACT_segment:
		gen_segment(action, column);
		return;
	case ACT_segment_length:
		gen_segment(action, column);
		return;
	case ACT_select:
		gen_select(action, column);
		break;
	case ACT_sql_dialect:
		sw_sql_dialect = ((SDT) action->act_object)->sdt_dialect;
		return;
	case ACT_start:
		gen_t_start(action, column);
		break;
	case ACT_statistics:
		gen_ddl(action, column);
		break;
	case ACT_store:
		gen_store(action, column);
		return;
	case ACT_store2:
		gen_return_value(action, column);
		return;
	case ACT_type_number:
		gen_type(action, column);
		return;
	case ACT_update:
		gen_update(action, column);
		break;
	case ACT_variable:
		gen_variable(action, column);
		return;
	default:
		return;
	};

//  Put in a trailing brace for those actions still with us 

	if (action->act_flags & ACT_sql)
		gen_whenever(action->act_whenever, column);

	if (action->act_error)
		ib_fprintf(out_file, ";");
	else
		endp(column);
}


//____________________________________________________________
//  
//		Align output to a specific column for output.  If the
//		column is negative, don't do anything.
//  

static void align( int column)
{
	if (column < 0)
		return;

	ib_putc('\n', out_file);

	int i;
	for (i = column / 8; i; --i)
		ib_putc('\t', out_file);

	for (i = column % 8; i; --i)
		ib_putc(' ', out_file);
}


//____________________________________________________________
//  
//		Build an assignment from a host language variable to
//		a port variable.   The string assignments are a little
//		hairy because the normal mode is varying (null 
//		terminated) strings, but the fixed subtype makes the
//		string a byte stream.  Furthering the complication, a
//		single character byte stream is handled as a single byte,
//		meaining that it is the byte, not the address of the
//		byte.
//  

static void asgn_from( const act* action, REF reference, int column)
{
	TEXT name[64], variable[20], temp[20];

	for (; reference; reference = reference->ref_next) {
		bool slice_flag = false;
		GPRE_FLD field = reference->ref_field;
		if (field->fld_array_info) {
		    act* slice_action;
		    REF source = reference->ref_friend;
			if (source &&
				(slice_action = (act*) source->ref_slice) &&
				slice_action->act_object)
			{
				slice_flag = true;
				slice_action->act_type = ACT_put_slice;
				gen_slice(slice_action, 0, column);
			}
			else if (!(reference->ref_flags & REF_array_elem)) {
				printa(column, "%s = isc_blob_null;",
					   gen_name(name, reference, true));
				gen_get_or_put_slice(action, reference, false, column);
				continue;
			}
		}
		if (!reference->ref_source && !reference->ref_value && !slice_flag)
			continue;
		align(column);
		gen_name(variable, reference, true);
		TEXT* value;
		if (slice_flag)
			value = gen_name(temp, reference->ref_friend, true);
		else if (reference->ref_source)
			value = gen_name(temp, reference->ref_source, true);
		else
			value = reference->ref_value;

		if (!slice_flag &&
			reference->ref_value &&
			(reference->ref_flags & REF_array_elem))
		{
			field = field->fld_array;
		}
		if (field && field->fld_dtype <= dtype_cstring)
		{
			if (field->fld_sub_type == 1)
				if (field->fld_length == 1)
					ib_fprintf(out_file, "%s = %s;", variable, value);
				else
					ib_fprintf(out_file,
							   "isc_ftof (%s, sizeof (%s), %s, %d);", value,
							   value, variable, field->fld_length);
			else if (field->fld_flags & FLD_dbkey)
				ib_fprintf(out_file, "isc_ftof (%s, %d, %s, %d);", value,
						   field->fld_length, variable, field->fld_length);
			else if (sw_cstring)
				ib_fprintf(out_file, "isc_vtov ((char*)%s, (char*)%s, %d);", value,
						   variable, field->fld_length);
			else if (reference->ref_source)
				ib_fprintf(out_file, "isc_ftof (%s, sizeof (%s), %s, %d);",
						   value, value, variable, field->fld_length);
			else
				ib_fprintf(out_file, "isc_vtof (%s, %s, %d);", value,
						   variable, field->fld_length);
		}
		else if (!reference->ref_master
				 || (reference->ref_flags & REF_literal))
		{
			ib_fprintf(out_file, "%s = %s;", variable, value);
		}
		else {
			ib_fprintf(out_file, "if (%s < 0)", value);
			align(column + 4);
			ib_fprintf(out_file, "%s = -1;", variable);
			align(column);
			ib_fprintf(out_file, "else");
			align(column + 4);
			ib_fprintf(out_file, "%s = 0;", variable);
		}
	}
}

//____________________________________________________________
//  
//		Build an assignment to a host language variable from
//		a port variable.
//  

static void asgn_to( const act* action, REF reference, int column)
{
	char s[64];

	REF source = reference->ref_friend;
	GPRE_FLD field = source->ref_field;

	if (field) {
	    act* slice_action;
		if (field->fld_array_info && (slice_action = (act*) source->ref_slice)) {
			source->ref_value = reference->ref_value;
			if (slice_action->act_object) {
				slice_action->act_type = ACT_get_slice;
				gen_slice(slice_action, source, column);
			}
			else
				gen_get_or_put_slice(action, source, true, column);

			// Pick up NULL value if one is there

			if (reference = reference->ref_null) {
				align(column);
				ib_fprintf(out_file, "%s = %s;", reference->ref_value,
						   gen_name(s, reference, true));
			}
			return;
		}

		gen_name(s, source, true);
		if (field->fld_dtype > dtype_cstring)
			ib_fprintf(out_file, "%s = %s;", reference->ref_value, s);
		else if (field->fld_sub_type == 1 && field->fld_length == 1)
			ib_fprintf(out_file, "%s = %s;", reference->ref_value, s);
		else if (field->fld_flags & FLD_dbkey)
			ib_fprintf(out_file, "isc_ftof (%s, %d, %s, %d);",
					   s, field->fld_length, reference->ref_value,
					   field->fld_length);
		else if (!sw_cstring || field->fld_sub_type == 1)
			ib_fprintf(out_file, "isc_ftof (%s, %d, %s, sizeof (%s));", s,
					   field->fld_length, reference->ref_value,
					   reference->ref_value);
		else
			ib_fprintf(out_file, "isc_vtov ((char*)%s, (char*)%s, sizeof (%s));", s,
					   reference->ref_value, reference->ref_value);
	}

//  Pick up NULL value if one is there 

	if (reference = reference->ref_null) {
		align(column);
		ib_fprintf(out_file, "%s = %s;", reference->ref_value,
				   gen_name(s, reference, true));
	}
}


//____________________________________________________________
//  
//		Build an assignment to a host language variable from
//		a port variable.
//  

static void asgn_to_proc(const ref* reference, int column)
{
	char s[64];

	for (; reference; reference = reference->ref_next) {
		if (!reference->ref_value)
			continue;
		const gpre_fld* field = reference->ref_field;
		gen_name(s, reference, true);
		align(column);

		if (field->fld_dtype > dtype_cstring)
			ib_fprintf(out_file, "%s = %s;", reference->ref_value, s);
		else if (field->fld_sub_type == 1 && field->fld_length == 1)
			ib_fprintf(out_file, "%s = %s;", reference->ref_value, s);
		else if (field->fld_flags & FLD_dbkey)
			ib_fprintf(out_file, "isc_ftof (%s, %d, %s, %d);",
					   s, field->fld_length, reference->ref_value,
					   field->fld_length);
		else if (!sw_cstring || field->fld_sub_type == 1)
			ib_fprintf(out_file, "isc_ftof (%s, %d, %s, sizeof (%s));", s,
					   field->fld_length, reference->ref_value,
					   reference->ref_value);
		else
			ib_fprintf(out_file, "isc_vtov ((char*)%s, (char*)%s, sizeof (%s));", s,
					   reference->ref_value, reference->ref_value);
	}
}


//____________________________________________________________
//  
//		Generate a function call for free standing ANY.  Somebody else
//		will need to generate the actual function.
//  

static void gen_any( const act* action, int column)
{
	align(column);
	GPRE_REQ request = action->act_request;

	ib_fprintf(out_file, "%s_r (&%s, &%s",
			   request->req_handle, request->req_handle, request->req_trans);

	POR port = request->req_vport;
	if (port)
		for (REF reference = port->por_references; reference;
			 reference = reference->ref_next)
		{
				ib_fprintf(out_file, ", %s", reference->ref_value);
		}

	ib_fprintf(out_file, ")");
}


//____________________________________________________________
//  
//		Generate code for AT END clause of FETCH.
//  

static void gen_at_end( const act* action, int column)
{
	char s[20];

	GPRE_REQ request = action->act_request;
	printa(column, "if (!%s) {", gen_name(s, request->req_eof, true));
}


//____________________________________________________________
//  
//		Substitute for a BASED ON <field name> clause.
//  

static void gen_based( const act* action, int column)
{
	TEXT s[64], *variable;
	USHORT datatype;
	SLONG length;
	DIM dimension;

	align(column);
	BAS based_on = (BAS) action->act_object;
	GPRE_FLD field = based_on->bas_field;

	if (based_on->bas_flags & BAS_segment) {
		datatype = (sw_cstring) ? dtype_cstring : dtype_text;
		if (!(length = field->fld_seg_length))
			length = 256;
		if (datatype == dtype_cstring)
			length++;
	}
	else if (field->fld_array_info)
		datatype = field->fld_array_info->ary_dtype;
	else
		datatype = field->fld_dtype;

	switch (datatype) {
	case dtype_short:
		ib_fprintf(out_file, "short");
		break;

	case dtype_long:
		ib_fprintf(out_file, DCL_LONG);
		break;

	case dtype_quad:
		ib_fprintf(out_file, DCL_QUAD);
		break;

// ** Begin date/time/timestamp *
	case dtype_sql_date:
		ib_fprintf(out_file, "ISC_DATE");
		break;

	case dtype_sql_time:
		ib_fprintf(out_file, "ISC_TIME");
		break;

	case dtype_timestamp:
		ib_fprintf(out_file, "ISC_TIMESTAMP");
		break;

	case dtype_int64:
		ib_fprintf(out_file, "ISC_INT64");
		break;

	case dtype_blob:
		ib_fprintf(out_file, "ISC_QUAD");
		break;

	case dtype_cstring:
	case dtype_text:
	case dtype_varying:
		ib_fprintf(out_file, "char");
		break;

	case dtype_real:
		ib_fprintf(out_file, "float");
		break;

	case dtype_double:
		ib_fprintf(out_file, "double");
		break;

	default:
		sprintf(s, "datatype %d unknown\n", field->fld_dtype);
		CPR_error(s);
		return;
	}

//  print the first variable, then precede the rest with commas 

	column += INDENT;

	// Notice this variable was named first_flag, same than the global variable.
	bool first = true;

	while (based_on->bas_variables) {
		variable = (TEXT*) MSC_pop(&based_on->bas_variables);
		if (!first)
			ib_fprintf(out_file, ",");
		first = false;
		align(column);
		ib_fprintf(out_file, "%s", variable);
		if (based_on->bas_flags & BAS_segment) {
			if (*variable != '*')
				ib_fprintf(out_file, "[%ld]", length);
		}
		else if (field->fld_array_info) {
			//  Print out the dimension part of the declaration

			for (dimension = field->fld_array_info->ary_dimension; dimension;
				 dimension = dimension->dim_next)
				ib_fprintf(out_file, " [%ld]",
						   dimension->dim_upper - dimension->dim_lower + 1);

			if (field->fld_array_info->ary_dtype <= dtype_varying &&
				field->fld_length > 1)
				ib_fprintf(out_file, " [%d]", field->fld_array->fld_length);
		}
		else
			if (*variable != '*' &&
				field->fld_dtype <= dtype_varying &&
				(field->fld_sub_type != 1 || field->fld_length > 1))
// *???????
// if (*variable != '*' && field->fld_dtype <= dtype_varying &&
//    field->fld_length > 1)
//  
			ib_fprintf(out_file, "[%d]", field->fld_length);
	}

	ib_fprintf(out_file, "%s\n", based_on->bas_terminator);
}


//____________________________________________________________
//  
//		Make a blob FOR loop.
//  

static void gen_blob_close( const act* action, USHORT column)
{
	BLB blob;
	PAT args;
	const TEXT* pattern1 = "isc_%IFcancel%ELclose%EN_blob (%V1, &%BH);";

	if (action->act_error)
		begin(column);

	if (action->act_flags & ACT_sql) {
		column = gen_cursor_close(action, action->act_request, column);
		blob = (BLB) action->act_request->req_blobs;
	}
	else
		blob = (BLB) action->act_object;

	args.pat_blob = blob;
	args.pat_vector1 = status_vector(action);
	args.pat_condition = (action->act_type == ACT_blob_cancel);
	PATTERN_expand(column, pattern1, &args);

	if (action->act_flags & ACT_sql) {
		endp(column);
		column -= INDENT;
	}

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		End a blob FOR loop.
//  

static void gen_blob_end( const act* action, USHORT column)
{
	BLB blob;
	PAT args;
	TEXT s1[32];
	const TEXT* pattern1 = "}\n\
isc_close_blob (%V1, &%BH);\n\
}";

	args.pat_blob = blob = (BLB) action->act_object;
	if (action->act_error) {
		sprintf(s1, "%s2", global_status_name);
		args.pat_vector1 = s1;
	}
	else
		args.pat_vector1 = status_vector(0);
	args.pat_condition = (action->act_type == ACT_blob_cancel);
	PATTERN_expand(column, pattern1, &args);
}


//____________________________________________________________
//  
//		Make a blob FOR loop.
//  

static void gen_blob_for( const act* action, USHORT column)
{
	PAT args;
	const TEXT* pattern1 = "%IFif (!%S1 [1]) {\n\
%ENwhile (1)\n\
   {";

	gen_blob_open(action, column);
	args.pat_condition = (action->act_error != NULL);
	args.pat_string1 = global_status_name;
	PATTERN_expand(column, pattern1, &args);
	column += INDENT;
	gen_get_segment(action, column);
	printa(column, "if (%s [1] && (%s [1] != isc_segment)) break;",
		   global_status_name, global_status_name);
}


//____________________________________________________________
//  
//		Generate the call to open (or create) a blob.
//  

static void gen_blob_open( const act* action, USHORT column)
{
	BLB blob;
	PAT args;
	REF reference;
	TEXT s[20];
	const TEXT* pattern1 =
		"isc_%IFcreate%ELopen%EN_blob2 (%V1, &%DH, &%RT, &%BH, &%FR, (short) %N1, %I1);";
	const TEXT* pattern2 =
		"isc_%IFcreate%ELopen%EN_blob2 (%V1, &%DH, &%RT, &%BH, &%FR, (short) 0, (%IFchar%ELunsigned char%EN*) 0);";

	if (sw_auto && (action->act_flags & ACT_sql)) {
		t_start_auto(action, action->act_request, status_vector(action),
					 column, true);
		printa(column, "if (%s)", request_trans(action, action->act_request));
		column += INDENT;
	}

	if ((action->act_error && (action->act_type != ACT_blob_for)) ||
		(action->act_flags & ACT_sql))
		begin(column);

	if (action->act_flags & ACT_sql) {
		column = gen_cursor_open(action, action->act_request, column);
		blob = (BLB) action->act_request->req_blobs;
		reference = ((OPN) action->act_object)->opn_using;
		gen_name(s, reference, true);
	}
	else {
		blob = (BLB) action->act_object;
		reference = blob->blb_reference;
	}

	args.pat_condition = (action->act_type == ACT_blob_create);	// open or create blob
	args.pat_vector1 = status_vector(action);	// status vector
	args.pat_database = blob->blb_request->req_database;	// database handle
	args.pat_request = blob->blb_request;	// transaction handle
	args.pat_blob = blob;		// blob handle
	args.pat_reference = reference;	// blob identifier
	args.pat_ident1 = blob->blb_bpb_ident;

	if ((action->act_flags & ACT_sql) && action->act_type == ACT_blob_open) {
		align(column);
		ib_fprintf(out_file, "%s = %s;", s, reference->ref_value);
	}

	if (args.pat_value1 = blob->blb_bpb_length)
		PATTERN_expand(column, pattern1, &args);
	else
		PATTERN_expand(column, pattern2, &args);

	if (action->act_flags & ACT_sql) {
		endp(column);
		column -= INDENT;
		endp(column);
		column -= INDENT;
		endp(column);
		if (sw_auto)
			column -= INDENT;
		set_sqlcode(action, column);
		if (action->act_type == ACT_blob_create) {
			printa(column, "if (!SQLCODE)");
			align(column + INDENT);
			ib_fprintf(out_file, "%s = %s;", reference->ref_value, s);
		}
	}
	else if ((action->act_error && (action->act_type != ACT_blob_for)))
		endp(column);
}


//____________________________________________________________
//  
//		Callback routine for BLR pretty printer.
//  

static void gen_blr(void* user_arg, SSHORT offset, const char* string)
{
	const char *q, *p1;
	char *q1, d, line[256];
	bool first_line = true;

	int indent = 2 * INDENT;
	const char* p = string;
	while (*p == ' ') {
		p++;
		indent++;
	}

//  Limit indentation to 192 characters 

	indent = MIN(indent, 192);

	int length = strlen(p);
	do {
		if (length + indent > 255) {
			// if we did not find somewhere to break between the 200th and 256th
			// character just print out 256 characters

			bool open_quote = false;
			for (q = p; (q - p + indent) < 255; q++) {
				if ((q - p + indent) > 220 && *q == ',' && !open_quote)
					break;
				if (*q == '\'' && *(q - 1) != '\\')
					open_quote = !open_quote;
			}
			++q;
		}
		else {
			q = p + strlen(p);
		}

		// Replace all occurrences of gds__ (or gds__) with isc_

		for (q1 = line, p1 = p; p1 < q;)
			if ((*q1++ = *p1++) == 'g')
				if (p1 < q && (*q1++ = *p1++) == 'd')
					if (p1 < q && (*q1++ = *p1++) == 's')
						if (p1 < q && (*q1++ = *p1++) == '_')
							if (p1 < q && ((d = *p1++) == '_' || d == '$'))
								strncpy(q1 - 4, "isc", 3);
							else
								*q1++ = d;
		*q1 = 0;
		printa(indent, line);
		length = length - (q - p);
		p = q;
		if (first_line) {
			indent = MIN(indent + INDENT, 192);
			first_line = false;
		}
	} while (length > 0);
}


//____________________________________________________________
//  
//		Zap all know handles.
//  

static void gen_clear_handles( const act* action, int column)
{
	for (GPRE_REQ request = requests; request; request = request->req_next) {
		if (!(request->req_flags & REQ_exp_hand))
			printa(column, "%s = 0;", request->req_handle);
	}
}


//____________________________________________________________
//  
//		Generate a symbol to ease compatibility with V3.
//  

static void gen_compatibility_symbol(
									 const TEXT* symbol,
									 const TEXT* v4_prefix, const TEXT* trailer)
{
	const char* v3_prefix = (isLangCpp(sw_language)) ? "gds_" : "gds__";
    //	v3_prefix = (sw_language == lang_cxx) ? "gds_" : "gds__";

	ib_fprintf(out_file, "#define %s%s\t%s%s%s\n", v3_prefix, symbol,
			   v4_prefix, symbol, trailer);
}


//____________________________________________________________
//  
//		Generate text to compile a request.
//  

static void gen_compile( const act* action, int column)
{
	PAT args;
	const TEXT* pattern1 =
		"isc_compile_request%IF2%EN (%V1, (FRBRD**) &%DH, (FRBRD**) &%RH, (short) sizeof (%RI), (char *) %RI);";
	const TEXT* pattern2 = "if (!%RH%IF && %S1%EN)";

	const gpre_req* request = action->act_request;
	args.pat_request = request;
	DBB db = request->req_database;
	args.pat_database = db;
	args.pat_vector1 = status_vector(action);
	args.pat_string1 = request_trans(action, request);
	args.pat_condition = (sw_auto && (action->act_error
							  || (action->act_flags & ACT_sql)));

	if (sw_auto)
		t_start_auto(action, request, status_vector(action), column, true);

	PATTERN_expand((USHORT) column, pattern2, &args);

	args.pat_condition = !(request->req_flags & REQ_exp_hand);
	args.pat_value1 = request->req_length;
	PATTERN_expand((USHORT) (column + INDENT), pattern1, &args);

//  If blobs are present, zero out all of the blob handles.  After this
//  point, the handles are the user's responsibility 

	const blb* blob = request->req_blobs;
	if (blob) {
		ib_fprintf(out_file, "\n");
		align(column - INDENT);
		for (; blob; blob = blob->blb_next)
			ib_fprintf(out_file, "isc_%d = ", blob->blb_ident);
		ib_fprintf(out_file, "0;");
	}
}


//____________________________________________________________
//  
//		Generate a call to create a database.
//  

static void gen_create_database( const act* action, int column)
{
	PAT args;
	const TEXT* pattern1 =
		"isc_create_database (%V1, %N1, \"%DF\", &%DH, %IF%S1, %S2%EL(short) 0, (char*) 0%EN, 0);";
	TEXT s1[32], s2[32], trname[32];

	GPRE_REQ request = ((MDBB) action->act_object)->mdbb_dpb_request;
	DBB db = (DBB) request->req_database;

	sprintf(s1, "isc_%dl", request->req_ident);
	sprintf(trname, "isc_%dt", request->req_ident);

	if (request->req_flags & REQ_extend_dpb) {
		sprintf(s2, "isc_%dp", request->req_ident);
		if (request->req_length)
			printa(column, "%s = isc_%d;", s2, request->req_ident);
		else
			printa(column, "%s = (char*) 0;", s2);

		printa(column,
			   "isc_expand_dpb (&%s, &%s, isc_dpb_user_name, %s, isc_dpb_password, %s, isc_dpb_sql_role_name, %s, isc_dpb_lc_messages, %s, isc_dpb_lc_ctype, %s, 0);",
			   s2, s1,
			   db->dbb_r_user ? db->dbb_r_user : "(char*) 0",
			   db->dbb_r_password ? db->dbb_r_password : "(char*) 0",
			   db->dbb_r_sql_role ? db->dbb_r_sql_role : "(char*) 0",
			   db->dbb_r_lc_messages ? db->dbb_r_lc_messages : "(char*) 0",
			   db->dbb_r_lc_ctype ? db->dbb_r_lc_ctype : "(char*) 0");
	}
	else
		sprintf(s2, "isc_%d", request->req_ident);

	args.pat_vector1 = status_vector(action);
	args.pat_request = request;
	args.pat_database = db;
	args.pat_value1 = strlen(db->dbb_filename);
	args.pat_condition = (request->req_length
						  || (request->req_flags & REQ_extend_dpb));
	args.pat_string1 = s1;
	args.pat_string2 = s2;

	PATTERN_expand((USHORT) column, pattern1, &args);

//  if the dpb was extended, free it here 

	if (request->req_flags & REQ_extend_dpb) {
		if (request->req_length)
			printa(column, "if (%s != isc_%d)", s2, request->req_ident);
		printa(column + (request->req_length ? INDENT : 0), "isc_free ((char*) %s);",
			   s2);

		// reset the length of the dpb

		printa(column, "%s = %d;", s1, request->req_length);
	}

	request = action->act_request;
	printa(column, "if (!%s [1])", global_status_name);
	column += INDENT;
	begin(column);
	printa(column,
		   "isc_start_transaction (%s, (FRBRD**) &%s, (short) 1, &%s, (short) 0, (char*) 0);",
		   status_vector(action), trname, db->dbb_name->sym_string);
	printa(column, "if (%s)", trname);
	column += INDENT;
	align(column);
	ib_fprintf(out_file, "isc_ddl (%s, &%s, &%s, (short) %d, isc_%d);",
			   status_vector(action),
			   request->req_database->dbb_name->sym_string,
			   trname, request->req_length, request->req_ident);
	column -= INDENT;
	printa(column, "if (!%s [1])", global_status_name);
	printa(column + INDENT, "isc_commit_transaction (%s, (FRBRD**) &%s);",
		   status_vector(action), trname);
	printa(column, "if (%s [1])", global_status_name);
	printa(column + INDENT, "isc_rollback_transaction (%s, (FRBRD**) &%s);",
		   status_vector(NULL), trname);
	set_sqlcode(action, column);
	endp(column);
	printa(column - INDENT, "else");
	set_sqlcode(action, column);
	column -= INDENT;
}


//____________________________________________________________
//  
//		Generate substitution text for END_STREAM.
//  

static int gen_cursor_close( const act* action, const gpre_req* request, int column)
{
	PAT args;
	const TEXT* pattern1 =
		"if (%RIs && !isc_dsql_free_statement (%V1, &%RIs, %N1))";

	args.pat_request = request;
	args.pat_vector1 = status_vector(action);
	args.pat_value1 = 1;

	PATTERN_expand((USHORT) column, pattern1, &args);
	column += INDENT;
	begin(column);

	return column;
}


//____________________________________________________________
//  
//		Generate text to initialize a cursor.
//  

static void gen_cursor_init( const act* action, int column)
{

//  If blobs are present, zero out all of the blob handles.  After this
//  point, the handles are the user's responsibility 

	if (action->act_request->
		req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
	{
		printa(column, "isc_%d = 0;",
			action->act_request->req_blobs->blb_ident);
	}
}


//____________________________________________________________
//  
//		Generate text to open an embedded SQL cursor.
//  

static int gen_cursor_open( const act* action, const gpre_req* request, int column)
{
	PAT args;
	TEXT s[64];
	const TEXT* pattern1 = "if (!%RIs && %RH%IF && %DH%EN)";
	const TEXT* pattern2 = "if (!%RIs%IF && %DH%EN)";
	const TEXT* pattern3 = "isc_dsql_alloc_statement2 (%V1, &%DH, &%RIs);";
	const TEXT* pattern4 = "if (%RIs%IF && %S3%EN)";
	const TEXT* pattern5 = "if (!isc_dsql_set_cursor_name (%V1, &%RIs, %S1, 0) &&";
	const TEXT* pattern6 =
		"!isc_dsql_execute_m (%V1, &%S3, &%RIs, 0, %S2, %N2, 0, %S2))";

	args.pat_request = request;
	args.pat_database = request->req_database;
	args.pat_vector1 = status_vector(action);
	args.pat_condition = sw_auto;
	args.pat_string1 = make_name(s, ((OPN) action->act_object)->opn_cursor);
	args.pat_string2 = NULL_STRING;
	args.pat_string3 = request_trans(action, request);
	args.pat_value2 = -1;

	PATTERN_expand((USHORT) column,
				   (action->act_type == ACT_open) ? pattern1 : pattern2,
				   &args);
	PATTERN_expand((USHORT) (column + INDENT), pattern3, &args);
	PATTERN_expand((USHORT) column, pattern4, &args);
	column += INDENT;
	begin(column);
	PATTERN_expand((USHORT) column, pattern5, &args);
	column += INDENT;
	PATTERN_expand((USHORT) column, pattern6, &args);
	begin(column);

	return column;
}


//____________________________________________________________
//  
//		Generate insertion text for the database statement.
//  

static void gen_database( const act* action, int column)
{
	if (global_first_flag)
		return;
	global_first_flag = true;

	ib_fprintf(out_file, "\n/**** GDS Preprocessor Definitions ****/\n");
	ib_fprintf(out_file, "#ifndef JRD_IBASE_H\n#include %s\n#endif\n",
			   GDS_INCLUDE);

	printa(column, "static %sISC_QUAD", CONST_STR);
	printa(column + INDENT,
		   "isc_blob_null = {0,0};\t/* initializer for blobs */");
	if (sw_language == lang_c)
		printa(column,
			   "static %slong *gds__null = 0;\t/* dummy status vector */",
			   CONST_STR);

	TEXT* scope = "";

	bool all_static = true;
	bool all_extern = true;

	DBB db;
	for (db = isc_databases; db; db = db->dbb_next) {
		all_static = all_static && (db->dbb_scope == DBB_STATIC);
		all_extern = all_extern && (db->dbb_scope == DBB_EXTERN);
		if (db->dbb_scope == DBB_STATIC)
			scope = "static ";
		else if (db->dbb_scope == DBB_EXTERN && TRUE)
			scope = "extern ";
		printa(column, "%sisc_db_handle", scope);
		if (!all_extern)
			printa(column + INDENT, "%s = 0;\t\t/* database handle */\n",
				   db->dbb_name->sym_string);
		else
			printa(column + INDENT, "%s;\t\t/* database handle */\n",
				   db->dbb_name->sym_string);
	}

	if (all_static)
		scope = "static ";
	else if (all_extern)
		scope = "extern ";

	printa(column, "%sisc_tr_handle", scope);
	if (!all_extern)
		printa(column + INDENT, "%s = 0;\t\t/* default transaction handle */",
			   transaction_name);
	else
		printa(column + INDENT, "%s;\t\t/* default transaction handle */",
			   transaction_name);

	printa(column, "%slong", scope);
	column += INDENT;
	printa(column, "%s [20],\t/* status vector */", global_status_name);
	printa(column, "%s2 [20];\t/* status vector */", global_status_name);
	printa(column - INDENT, "%s%s", scope, DCL_LONG);
	printa(column, "isc_array_length, \t/* array return size */");
	printa(column, "SQLCODE;\t\t/* SQL status code */");
	if (sw_dyn_using) {
		printa(column - INDENT,
			   "static struct isc_sqlda { /* SQLDA for internal use */");
		printa(column, "char	sqldaid [8];");
		printa(column, "%s	sqldabc;", DCL_LONG);
		printa(column, "short	sqln;");
		printa(column, "short	sqld;");
		printa(column, "SQLVAR	sqlvar[%d];", sw_dyn_using);
		printa(column, "}");
	}

	column -= INDENT;

	for (db = isc_databases; db; db = db->dbb_next)
		for (tpb* tpb_iterator = db->dbb_tpbs;
			 tpb_iterator;
			 tpb_iterator = tpb_iterator->tpb_dbb_next)
		{
			gen_tpb(tpb_iterator, column);
		}

//  generate event parameter block for each event in module 

	SSHORT max_count = 0;
	for (LLS stack_ptr = events; stack_ptr; stack_ptr = stack_ptr->lls_next) {
		SSHORT count = gen_event_block((const act*) (stack_ptr->lls_object));
		max_count = MAX(count, max_count);
	}

	if (max_count)
		printa(column, "%s%s isc_events [%d];\t/* event vector */", scope,
			   DCL_LONG, max_count);

	for (GPRE_REQ request = requests; request; request = request->req_next) {
		gen_request(request);

		//  Array declarations

		if (POR port = request->req_primary)
			for (REF reference = port->por_references; reference;
				 reference = reference->ref_next)
			{
					if (reference->ref_flags & REF_fetch_array)
						make_array_declaration(reference);
			}
	}

	ib_fprintf(out_file, "\n\n");
	gen_compatibility_symbol("blob_null", "isc_",
							 "\t/* compatibility symbols */");
// ****
//gen_compatibility_symbol ("database", "isc_", "");
//gen_compatibility_symbol ("trans", "isc_", "");
//***
	gen_compatibility_symbol("status", "isc_", "");
	gen_compatibility_symbol("status2", "isc_", "");
	gen_compatibility_symbol("array_length", "isc_", "");
	if (max_count)
		gen_compatibility_symbol("events", "isc_", "");
	gen_compatibility_symbol("count", "isc_", "");
	gen_compatibility_symbol("slack", "isc_", "");
	gen_compatibility_symbol("utility", "isc_",
							 "\t/* end of compatibility symbols */");

	ib_fprintf(out_file, "\n#ifndef isc_version4\n");
	ib_fprintf(out_file, "    Generate a compile-time error.\n");
	ib_fprintf(out_file,
			   "    Picking up a V3 include file after preprocessing with V4 GPRE.\n");
	ib_fprintf(out_file, "#endif\n");
	ib_fprintf(out_file, "\n/**** end of GPRE definitions ****/\n");
}


//____________________________________________________________
//  
//		Generate a call to update metadata.
//  

static void gen_ddl( const act* action, int column)
{
//  Set up command type for call to RDB$DDL

	const gpre_req* request = action->act_request;

	if (sw_auto) {
		t_start_auto(action, 0, status_vector(action), column, true);
		printa(column, "if (%s)", transaction_name);
		column += INDENT;
	}

	align(column);
	ib_fprintf(out_file, "isc_ddl (%s, &%s, &%s, (short) %d, isc_%d);",
			   status_vector(action),
			   request->req_database->dbb_name->sym_string,
			   transaction_name, request->req_length, request->req_ident);

	if (sw_auto) {
		column -= INDENT;
		printa(column, "if (!%s [1])", global_status_name);
		printa(column + INDENT, "isc_commit_transaction (%s, (FRBRD**) &%s);",
			   status_vector(action), transaction_name);
		printa(column, "if (%s [1])", global_status_name);
		printa(column + INDENT, "isc_rollback_transaction (%s, (FRBRD**) &%s);",
			   status_vector(NULL), transaction_name);
	}

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a call to create a database.
//  

static void gen_drop_database( const act* action, int column)
{
	DBB db = (DBB) action->act_object;
	align(column);

	ib_fprintf(out_file,
			   "isc_drop_database (%s, %d, \"%s\", rdb$k_db_type_gds);",
			   status_vector(action),
			   strlen(db->dbb_filename), db->dbb_filename);
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_close( const act* action, int column)
{
	TEXT s[64];

	const dyn* statement = (DYN) action->act_object;
	printa(column,
		   "isc_embed_dsql_close (%s, %s);",
		   global_status_name, make_name(s, statement->dyn_cursor_name));
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_declare( const act* action, int column)
{
	TEXT s1[64], s2[64];

	const dyn* statement = (DYN) action->act_object;
	printa(column,
		   "isc_embed_dsql_declare (%s, %s, %s);",
		   global_status_name,
		   make_name(s1, statement->dyn_statement_name),
		   make_name(s2, statement->dyn_cursor_name));
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_describe(const act* action,
							 int column,
							 bool bind_flag)
{
	TEXT s[64];

	const dyn* statement = (DYN) action->act_object;
	printa(column,
		   "isc_embed_dsql_describe%s (%s, %s, %d, %s);",
		   bind_flag ? "_bind" : "",
		   global_status_name,
		   make_name(s, statement->dyn_statement_name),
		   sw_sql_dialect, statement->dyn_sqlda);
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_execute( const act* action, int column)
{
	TEXT s[64];
	gpre_req* request;
	gpre_req req_const;

	DYN statement = (DYN) action->act_object;
	const TEXT* transaction;
	if (statement->dyn_trans) {
		transaction = statement->dyn_trans;
		request = &req_const;
		request->req_trans = transaction;
	}
	else {
		transaction = transaction_name;
		request = NULL;
	}

	if (sw_auto) {
		t_start_auto(action, request, status_vector(action), column, true);
		printa(column, "if (%s)", transaction);
		column += INDENT;
	}

	if (statement->dyn_sqlda2)
		printa(column,
			   "isc_embed_dsql_execute2 (%s, &%s, %s, %d, %s, %s);",
			   global_status_name,
			   transaction,
			   make_name(s, statement->dyn_statement_name),
			   sw_sql_dialect,
			   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_STRING,
			   statement->dyn_sqlda2);
	else
		printa(column,
			   "isc_embed_dsql_execute (%s, &%s, %s, %d, %s);",
			   global_status_name,
			   transaction,
			   make_name(s, statement->dyn_statement_name),
			   sw_sql_dialect,
			   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_STRING);

	if (sw_auto)
		column -= INDENT;

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_fetch( const act* action, int column)
{
	TEXT s[64];

	const dyn* statement = (DYN) action->act_object;
	printa(column, "SQLCODE = isc_embed_dsql_fetch (%s, %s, %d, %s);",
		   global_status_name, make_name(s, statement->dyn_cursor_name),
		   sw_sql_dialect,
		   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_SQLDA);

	printa(column, "if (SQLCODE != 100) SQLCODE = isc_sqlcode (%s);",
		   global_status_name);
}


//____________________________________________________________
//  
//		Generate code for an EXECUTE IMMEDIATE dynamic SQL statement.
//  

static void gen_dyn_immediate( const act* action, int column)
{
	gpre_req* request;
	gpre_req req_const;

	DYN statement = (DYN) action->act_object;
	DBB database = statement->dyn_database;
	const TEXT* transaction;
	if (statement->dyn_trans) {
		transaction = statement->dyn_trans;
		request = &req_const;
		request->req_trans = transaction;
	}
	else {
		transaction = transaction_name;
		request = NULL;
	}

	if (sw_auto) {
		t_start_auto(action, request, status_vector(action), column, true);
		printa(column, "if (%s)", transaction);
		column += INDENT;
	}

	printa(column,
		   (statement->dyn_sqlda2) ?
		   "isc_embed_dsql_execute_immed2 (%s, &%s, &%s, 0, %s, %d, %s, %s);"
		   : "isc_embed_dsql_execute_immed (%s, &%s, &%s, 0, %s, %d, %s);",
		   global_status_name, database->dbb_name->sym_string, transaction,
		   statement->dyn_string, sw_sql_dialect,
		   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_SQLDA,
		   (statement->dyn_sqlda2) ? statement->dyn_sqlda2 : NULL_SQLDA);

	if (sw_auto)
		column -= INDENT;

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_insert( const act* action, int column)
{
	TEXT s[64];

	const dyn* statement = (DYN) action->act_object;
	printa(column,
		   "isc_embed_dsql_insert (%s, %s, %d, %s);",
		   global_status_name,
		   make_name(s, statement->dyn_cursor_name),
		   sw_sql_dialect,
		   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_SQLDA);

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_open( const act* action, int column)
{
	TEXT s[64];
	gpre_req* request;
	gpre_req req_const;

	DYN statement = (DYN) action->act_object;
	const TEXT* transaction;
	if (statement->dyn_trans) {
		transaction = statement->dyn_trans;
		request = &req_const;
		request->req_trans = transaction;
	}
	else {
		transaction = transaction_name;
		request = NULL;
	}

	if (sw_auto) {
		t_start_auto(action, request, status_vector(action), column, true);
		printa(column, "if (%s)", transaction);
		column += INDENT;
	}

	make_name(s, statement->dyn_cursor_name);

	printa(column,
		   (statement->dyn_sqlda2) ?
		   "isc_embed_dsql_open2 (%s, &%s, %s, %d, %s, %s);" :
		   "isc_embed_dsql_open (%s, &%s, %s, %d, %s);",
		   global_status_name,
		   transaction,
		   s,
		   sw_sql_dialect,
		   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_SQLDA,
		   (statement->dyn_sqlda2) ? statement->dyn_sqlda2 : NULL_SQLDA);

	if (sw_auto)
		column -= INDENT;

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a dynamic SQL statement.
//  

static void gen_dyn_prepare( const act* action, int column)
{
	TEXT s[64];
	gpre_req* request;
	gpre_req req_const;

	DYN statement = (DYN) action->act_object;
	const TEXT* transaction;
	if (statement->dyn_trans) {
		transaction = statement->dyn_trans;
		request = &req_const;
		request->req_trans = transaction;
	}
	else {
		transaction = transaction_name;
		request = NULL;
	}

	if (sw_auto) {
		t_start_auto(action, request, status_vector(action), column, true);
		printa(column, "if (%s)", transaction);
		column += INDENT;
	}

    DBB database = statement->dyn_database;
	printa(column,
		   "isc_embed_dsql_prepare (%s, &%s, &%s, %s, 0, %s, %d, %s);",
		   global_status_name, database->dbb_name->sym_string, transaction,
		   make_name(s, statement->dyn_statement_name), statement->dyn_string,
		   sw_sql_dialect,
		   (statement->dyn_sqlda) ? statement->dyn_sqlda : NULL_SQLDA);

	if (sw_auto)
		column -= INDENT;

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate substitution text for END_MODIFY.
//  
//		Trickier because a fixed subtype single character
//		field is a single character, not a pointer to a
//		single character.
//  

static void gen_emodify( const act* action, int column)
{
	TEXT s1[20], s2[20];

	UPD modify = (UPD) action->act_object;

	for (REF reference = modify->upd_port->por_references; reference;
		 reference = reference->ref_next)
	{
	    REF source = reference->ref_source;
		if (!source)
			continue;
		GPRE_FLD field = reference->ref_field;
		align(column);
		gen_name(s1, source, true);
		gen_name(s2, reference, true);
		if (field->fld_dtype > dtype_cstring ||
			(field->fld_sub_type == 1 && field->fld_length == 1))
			ib_fprintf(out_file, "%s = %s;", s2, s1);
		else if (sw_cstring && !field->fld_sub_type)
			ib_fprintf(out_file, "isc_vtov ((char*)%s, (char*)%s, %d);",
					   s1, s2, field->fld_length);
		else
			ib_fprintf(out_file, "isc_ftof (%s, %d, %s, %d);",
					   s1, field->fld_length, s2, field->fld_length);
		if (field->fld_array_info)
			gen_get_or_put_slice(action, reference, false, column);
	}

	gen_send(action, modify->upd_port, column);

}


//____________________________________________________________
//  
//		Generate substitution text for END_STORE.
//  

static void gen_estore( const act* action, int column)
{
	GPRE_REQ request = action->act_request;

//  if we did a store ... returning_values aka store2
//  just wrap up pending error and return 

	if (request->req_type == REQ_store2) {
		if (action->act_error)
			endp(column);
		return;
	}

	if (action->act_error)
		column += INDENT;

	align(column);
	gen_start(action, request->req_primary, column, true);

	if (action->act_error)
		endp(column);
}


//____________________________________________________________
//  
//		Generate definitions associated with a single request.
//  

static void gen_endfor( const act* action, int column)
{
	GPRE_REQ request = action->act_request;
	column += INDENT;

	if (request->req_sync)
		gen_send(action, request->req_sync, column);

	endp(column);

	if (action->act_error || (action->act_flags & ACT_sql))
		endp(column);
}


//____________________________________________________________
//  
//		Generate substitution text for ERASE.
//  

static void gen_erase( const act* action, int column)
{
	if (action->act_error || (action->act_flags & ACT_sql))
		begin(column);

	UPD erase = (UPD) action->act_object;
	gen_send(action, erase->upd_port, column);

	if (action->act_flags & ACT_sql)
		endp(column);
}


//____________________________________________________________
//  
//		Generate event parameter blocks for use
//		with a particular call to isc_event_wait.
//  

static SSHORT gen_event_block( const act* action)
{
	GPRE_NOD init = (GPRE_NOD) action->act_object;
	SYM event_name = (SYM) init->nod_arg[0];

	int ident = CMP_next_ident();
	init->nod_arg[2] = (GPRE_NOD) ident;

	printa(0, "static %schar\n   *isc_%da, *isc_%db;", CONST_STR, ident,
		   ident);
	printa(0, "static short\n   isc_%dl;", ident);

	GPRE_NOD list = init->nod_arg[1];
	return list->nod_count;
}


//____________________________________________________________
//  
//		Generate substitution text for EVENT_INIT.
//  

static void gen_event_init( const act* action, int column)
{
	GPRE_NOD *ptr, *end, node;
	REF reference;
	PAT args;
	TEXT variable[20];
	const TEXT* pattern1 =
		"isc_%N1l = isc_event_block (&isc_%N1a, &isc_%N1b, (short) %N2";
	const TEXT* pattern2 =
		"isc_wait_for_event (%V1, &%DH, isc_%N1l, isc_%N1a, isc_%N1b);";
	const TEXT* pattern3 =
		"isc_event_counts (isc_events, isc_%N1l, isc_%N1a, isc_%N1b);";

	if (action->act_error)
		begin(column);
	begin(column);

	GPRE_NOD init = (GPRE_NOD) action->act_object;
	GPRE_NOD event_list = init->nod_arg[1];

	args.pat_database = (DBB) init->nod_arg[3];
	args.pat_vector1 = status_vector(action);
	args.pat_value1 = (int) init->nod_arg[2];
	args.pat_value2 = (int) event_list->nod_count;

//  generate call to dynamically generate event blocks 

	PATTERN_expand((USHORT) column, pattern1, &args);

	for (ptr = event_list->nod_arg, end = ptr + event_list->nod_count;
		 ptr < end; ptr++) {
		node = *ptr;
		if (node->nod_type == nod_field) {
			reference = (REF) node->nod_arg[0];
			gen_name(variable, reference, true);
			printb(", %s", variable);
		}
		else
			printb(", %s", (TEXT* ) node->nod_arg[0]);
	}

	printb(");");

//  generate actual call to event_wait 

	PATTERN_expand((USHORT) column, pattern2, &args);

//  get change in event counts, copying event parameter block for reuse 

	PATTERN_expand((USHORT) column, pattern3, &args);

	if (action->act_error)
		endp(column);
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate substitution text for EVENT_WAIT.
//  

static void gen_event_wait( const act* action, int column)
{
	PAT args;
	TEXT s[64];
	const TEXT* pattern1 =
		"isc_wait_for_event (%V1, &%DH, isc_%N1l, isc_%N1a, isc_%N1b);";
	const TEXT* pattern2 =
		"isc_event_counts (isc_events, isc_%N1l, isc_%N1a, isc_%N1b);";

	if (action->act_error)
		begin(column);
	begin(column);

	SYM event_name = (SYM) action->act_object;

//  go through the stack of events, checking to see if the
//  event has been initialized and getting the event identifier 

	DBB database;
	int ident = -1;
	for (LLS stack_ptr = events; stack_ptr; stack_ptr = stack_ptr->lls_next) {
		const act* event_action = (const act*) stack_ptr->lls_object;
		GPRE_NOD event_init = (GPRE_NOD) event_action->act_object;
		SYM stack_name = (SYM) event_init->nod_arg[0];
		if (!strcmp(event_name->sym_string, stack_name->sym_string)) {
			ident = (int) event_init->nod_arg[2];
			database = (DBB) event_init->nod_arg[3];
		}
	}

	if (ident < 0) {
		sprintf(s, "event handle \"%s\" not found", event_name->sym_string);
		CPR_error(s);
		return;
	}

	args.pat_database = database;
	args.pat_vector1 = status_vector(action);
	args.pat_value1 = (int) ident;

//  generate calls to wait on the event and to fill out the events array 

	PATTERN_expand((USHORT) column, pattern1, &args);
	PATTERN_expand((USHORT) column, pattern2, &args);

	if (action->act_error)
		endp(column);
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate replacement text for the SQL FETCH statement.  The
//		epilog FETCH statement is handled by GEN_S_FETCH (generate
//		stream fetch).
//  

static void gen_fetch( const act* action, int column)
{
	TEXT s[20];

	GPRE_REQ request = action->act_request;

#ifdef SCROLLABLE_CURSORS
	POR port = request->req_aport;
	if (port) {
		// set up the reference to point to the correct value
		//   in the linked list of values, and prepare for the
		//   next FETCH statement if applicable

		REF reference;
		for (reference = port->por_references; reference;
			reference = reference->ref_next)
		{
			VAL value = reference->ref_values;
			reference->ref_value = value->val_value;
			reference->ref_values = value->val_next;
		}

		// find the direction and offset parameters

		reference = port->por_references;
		SCHAR* offset = reference->ref_value;
		reference = reference->ref_next;
		SCHAR* direction = reference->ref_value;

		/* the direction in which the engine will scroll is sticky, so check to see 
		   the last direction passed to the engine; if the direction is the same and 
		   the offset is 1, then there is no need to pass the message; this prevents 
		   extra packets and allows for batch fetches in either direction */

		printa(column, "if (isc_%ddirection %% 2 != %s || %s != 1)",
			   request->req_ident, direction, offset);
		column += INDENT;
		begin(column);

		// assign the direction and offset parameters to the appropriate message,
		// then send the message to the engine

		asgn_from(action, port->por_references, column);
		gen_send(action, port, column);
		printa(column, "isc_%ddirection = %s;", request->req_ident,
			   direction);
		column -= INDENT;
		endp(column);

		printa(column, "if (!SQLCODE)");
		column += INDENT;
		begin(column);
	}
#endif

	if (request->req_sync) {
		gen_send(action, request->req_sync, column);
		printa(column, "if (!SQLCODE)");
		column += INDENT;
		begin(column);
	}

	gen_receive(action, column, request->req_primary);
	printa(column, "if (!SQLCODE)");
	column += INDENT;
	printa(column, "if (%s)", gen_name(s, request->req_eof, true));
	column += INDENT;
	begin(column);

	GPRE_NOD var_list = (GPRE_NOD) action->act_object;
	if (var_list)
		for (int i = 0; i < var_list->nod_count; i++) {
			align(column);
			asgn_to(action, (REF) (var_list->nod_arg[i]), column);
		}
	endp(column);
	printa(column - INDENT, "else");
	printa(column, "SQLCODE = 100;");

	if (request->req_sync) {
		column -= INDENT;
		endp(column);
	}

#ifdef SCROLLABLE_CURSORS
	if (port) {
		column -= INDENT;
		endp(column);
	}
#endif
}


//____________________________________________________________
//  
//		Generate substitution text for FINISH
//  

static void gen_finish( const act* action, int column)
{
	PAT args;
	const TEXT* pattern1 = "if (%S2)\n\
    isc_%S1_transaction (%V1, (FRBRD**) &%S2);";

	args.pat_vector1 = status_vector(action);
	args.pat_string2 = transaction_name;

	if (sw_auto || ((action->act_flags & ACT_sql) &&
					(action->act_type != ACT_disconnect))) {
		args.pat_string1 = (TEXT*)
		   ((action->act_type != ACT_rfinish) ? "commit" : "rollback");
		PATTERN_expand((USHORT) column, pattern1, &args);
	}

	DBB db = NULL;

//  the user supplied one or more db_handles 

	for (RDY ready = (RDY) action->act_object; ready; ready = ready->rdy_next) {
		db = ready->rdy_database;
		printa(column, "isc_detach_database (%s, &%s);",
			   status_vector(action), db->dbb_name->sym_string);
	}
//  no hanbdles, so we finish all known databases 

	if (!db)
		for (db = isc_databases; db; db = db->dbb_next) {
			if ((action->act_error || (action->act_flags & ACT_sql)) &&
				(db != isc_databases))
					printa(column, "if (%s && !%s [1]) ",
						   db->dbb_name->sym_string, global_status_name);
			else
				printa(column, "if (%s)", db->dbb_name->sym_string);
			printa(column + INDENT, "isc_detach_database (%s, &%s);",
				   status_vector(action), db->dbb_name->sym_string);
		}

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate substitution text for FOR statement.
//  

static void gen_for( const act* action, int column)
{
	TEXT s[20];

	gen_s_start(action, column);
	GPRE_REQ request = action->act_request;

	if (action->act_error || (action->act_flags & ACT_sql))
		printa(column, "if (!%s [1]) {", global_status_name);

	printa(column, "while (1)");
	column += INDENT;
	begin(column);
	gen_receive(action, column, request->req_primary);

	if (action->act_error || (action->act_flags & ACT_sql))
		printa(column, "if (!%s || %s [1]) break;",
			   gen_name(s, request->req_eof, true), global_status_name);
	else
		printa(column, "if (!%s) break;",
			   gen_name(s, request->req_eof, true));

	if (POR port = action->act_request->req_primary)
		for (REF reference = port->por_references; reference;
			 reference = reference->ref_next)
		{
			if (reference->ref_field->fld_array_info)
				gen_get_or_put_slice(action, reference, true, column);
		}

}


//____________________________________________________________
//  
//		Generate a function for free standing ANY or statistical.
//  

static void gen_function( const act* function, int column)
{
	REF reference;
	TEXT s[64];

	const act* action = (const act*) function->act_object;

	if (action->act_type != ACT_any) {
		CPR_error("can't generate function");
		return;
	}

	GPRE_REQ request = action->act_request;

	ib_fprintf(out_file, "static %s_r (request, transaction ", request->req_handle);

	POR port = request->req_vport;
	if (port)
		for (reference = port->por_references; reference;
			 reference = reference->ref_next)
		{
			ib_fprintf(out_file, ", %s",
					   gen_name(s, reference->ref_source, true));
		}

	ib_fprintf(out_file,
			   ")\n    isc_req_handle\trequest;\n    isc_tr_handle\ttransaction;\n");

	if (port)
		for (reference = port->por_references; reference;
			 reference = reference->ref_next)
		{
			const TEXT* dtype;
			GPRE_FLD field = reference->ref_field;
			switch (field->fld_dtype) {
			case dtype_short:
				dtype = "short";
				break;

			case dtype_long:
				dtype = DCL_LONG;
				break;

			case dtype_cstring:
			case dtype_text:
				dtype = "char*";
				break;

			case dtype_quad:
				dtype = DCL_QUAD;
				break;

// ** Begin date/time/timestamp *
			case dtype_sql_date:
				dtype = "ISC_DATE";
				break;

			case dtype_sql_time:
				dtype = "ISC_TIME";
				break;

			case dtype_timestamp:
				dtype = "ISC_TIMESTAMP";
				break;
// ** End date/time/timestamp *

			case dtype_int64:
				dtype = "ISC_INT64";
				break;

			case dtype_blob:
				dtype = "ISC_QUAD";
				break;

			case dtype_real:
				dtype = "float";
				break;

			case dtype_double:
				dtype = "double";
				break;

			default:
				CPR_error("gen_function: unsupported datatype");
				return;
			}
			ib_fprintf(out_file, "    %s\t%s;\n", dtype,
					   gen_name(s, reference->ref_source, true));
		}

	ib_fprintf(out_file, "{\n");
	for (port = request->req_ports; port; port = port->por_next)
		make_port(port, column);

	ib_fprintf(out_file, "\n\n");
	gen_s_start(action, 0);
	gen_receive(action, column, request->req_primary);

	for (port = request->req_ports; port; port = port->por_next)
		for (reference = port->por_references; reference;
			 reference = reference->ref_next)
		{
			if (reference->ref_field->fld_array_info)
				gen_get_or_put_slice(action, reference, true, column);
		}

	port = request->req_primary;
	ib_fprintf(out_file, "\nreturn %s;\n}\n",
			   gen_name(s, port->por_references, true));
}


//____________________________________________________________
//  
//		Generate a call to isc_get_slice
//       or isc_put_slice for an array.
//  

static void gen_get_or_put_slice(const act* action,
								 REF reference,
								 bool get,
								 int column)
{
	PAT args;
	TEXT s1[25], s2[10], s4[10];
	const TEXT* pattern1 =
		"isc_get_slice (%V1, &%DH, &%RT, &%S2, (short) %N1, (char *) %S3, 0, (%S6*) 0, (%S6) %L1, %S5, &isc_array_length);";
	const TEXT* pattern2 =
		"isc_put_slice (%V1, &%DH, &%RT, &%S2, (short) %N1, (char *) %S3, 0, (%S6*) 0, (%S6) %L1, (void *)%S5);";

	if (!(reference->ref_flags & REF_fetch_array))
		return;

	args.pat_request = action->act_request;
	args.pat_condition = get;	// get or put slice
	args.pat_vector1 = status_vector(action);	// status vector
	args.pat_database = action->act_request->req_database;	// database handle
	gen_name(s1, reference, true);	// blob handle
	args.pat_string2 = s1;
	args.pat_value1 = reference->ref_sdl_length;	// slice description length
	sprintf(s2, "isc_%d", reference->ref_sdl_ident);	// slice description
	args.pat_string3 = s2;

	args.pat_long1 = reference->ref_field->fld_array_info->ary_size;
	// slice size

	if (action->act_flags & ACT_sql)
		args.pat_string5 = reference->ref_value;
	else {
		sprintf(s4, "isc_%d",
				reference->ref_field->fld_array_info->ary_ident);
		args.pat_string5 = s4;	// array name
	}

	args.pat_string6 = DCL_LONG;

	PATTERN_expand((USHORT) column, (get) ? pattern1 : pattern2, &args);

	set_sqlcode(action, column);
	if (action->act_flags & ACT_sql)
		gen_whenever(action->act_whenever, column);
}


//____________________________________________________________
//  
//		Generate the code to do a get segment.
//  

static void gen_get_segment( const act* action, int column)
{
	BLB blob;
	PAT args;
	const TEXT* pattern1 =
		"%IF%S1 [1] = %ENisc_get_segment (%V1, &%BH, &%I1, (short) sizeof (%I2), %I2);";

	if (action->act_error && (action->act_type != ACT_blob_for))
		begin(column);

	if (action->act_flags & ACT_sql)
		blob = (BLB) action->act_request->req_blobs;
	else
		blob = (BLB) action->act_object;

	args.pat_blob = blob;
	args.pat_vector1 = status_vector(action);
	args.pat_condition = !(action->act_error
						   || (action->act_flags & ACT_sql));
	args.pat_ident1 = blob->blb_len_ident;
	args.pat_ident2 = blob->blb_buff_ident;
	args.pat_string1 = global_status_name;
	PATTERN_expand((USHORT) column, pattern1, &args);

	if (action->act_flags & ACT_sql) {
		REF into = action->act_object;
		set_sqlcode(action, column);
		printa(column, "if (!SQLCODE || SQLCODE == 101)");
		column += INDENT;
		begin(column);
		align(column);
		ib_fprintf(out_file, "isc_ftof (isc_%d, isc_%d, %s, isc_%d);",
				   blob->blb_buff_ident, blob->blb_len_ident,
				   into->ref_value, blob->blb_len_ident);
		if (into->ref_null_value) {
			align(column);
			ib_fprintf(out_file, "%s = isc_%d;",
					   into->ref_null_value, blob->blb_len_ident);
		}
		endp(column);
		column -= INDENT;
	}
}


//____________________________________________________________
//  
//		Generate text to compile and start a SQL command
//  

static void gen_loop( const act* action, int column)
{
	TEXT name[20];

	gen_s_start(action, column);
	GPRE_REQ request = action->act_request;
	POR port = request->req_primary;
	printa(column, "if (!SQLCODE) ");
	column += INDENT;
	begin(column);
	gen_receive(action, column, port);
	gen_name(name, port->por_references, true);
	printa(column, "if (!SQLCODE && !%s)", name);
	printa(column + INDENT, "SQLCODE = 100;");
	endp(column);
	column -= INDENT;
}


//____________________________________________________________
//  
//		Generate a name for a reference.  Name is constructed from
//		port and parameter idents.
//  

static TEXT* gen_name(char *string,
					  const ref* reference,
					  bool as_blob)
{

	if (reference->ref_field->fld_array_info && !as_blob)
		sprintf(string, "isc_%d",
				reference->ref_field->fld_array_info->ary_ident);
	else if (reference->ref_port)
		sprintf(string, "isc_%d.isc_%d",
				reference->ref_port->por_ident, reference->ref_ident);
	else
		sprintf(string, "isc_%d", reference->ref_ident);

	return string;
}


//____________________________________________________________
//  
//		Generate a block to handle errors.
//  

static void gen_on_error( const act* action, USHORT column)
{
	const act* err_action = (const act*) action->act_object;
	if ((err_action->act_type == ACT_get_segment) ||
		(err_action->act_type == ACT_put_segment) ||
		(err_action->act_type == ACT_endblob))
			printa(column,
				   "if (%s [1] && (%s [1] != isc_segment) && (%s [1] != isc_segstr_eof))",
				   global_status_name, global_status_name, global_status_name);
	else
		printa(column, "if (%s [1])", global_status_name);
	column += INDENT;
	begin(column);
}


//____________________________________________________________
//  
//		Generate code for an EXECUTE PROCEDURE.
//  

static void gen_procedure( const act* action, int column)
{
	column += INDENT;
	GPRE_REQ request = action->act_request;
	POR in_port = request->req_vport;
	POR out_port = request->req_primary;

	const dbb* database = request->req_database;
	PAT args;
	args.pat_database = database;
	args.pat_request = action->act_request;
	args.pat_vector1 = status_vector(action);
	args.pat_request = request;
	args.pat_port = in_port;
	args.pat_port2 = out_port;
	const TEXT* pattern;
	if (in_port && in_port->por_length)
		pattern =
			"isc_transact_request (%V1, %RF%DH%RE, %RF%RT%RE, sizeof (%RI), %RI, (short) %PL, (char *) %RF%PI%RE, (short) %QL, (char *) %RF%QI%RE);";
	else
		pattern =
			"isc_transact_request (%V1, %RF%DH%RE, %RF%RT%RE, sizeof (%RI), %RI, 0, 0, (short) %QL, (char *) %RF%QI%RE);";

//  Get database attach and transaction started 

	if (sw_auto)
		t_start_auto(action, 0, status_vector(action), column, true);

//  Move in input values 

	asgn_from(action, request->req_values, column);

//  Execute the procedure 

	PATTERN_expand((USHORT) column, pattern, &args);

	set_sqlcode(action, column);

	printa(column, "if (!SQLCODE)");
	column += INDENT;
	begin(column);

//  Move out output values 

	asgn_to_proc(request->req_references, column);
	endp(column);
}


//____________________________________________________________
//  
//		Generate the code to do a put segment.
//  

static void gen_put_segment( const act* action, int column)
{
	BLB blob;
	PAT args;
	const TEXT* pattern1 = "%IF%S1 [1] = %ENisc_put_segment (%V1, &%BH, %I1, %I2);";

	if (!action->act_error)
		begin(column);
	if (action->act_error || (action->act_flags & ACT_sql))
		begin(column);

	if (action->act_flags & ACT_sql) {
		blob = (BLB) action->act_request->req_blobs;
		REF from = action->act_object;
		align(column);
		ib_fprintf(out_file, "isc_%d = %s;",
				   blob->blb_len_ident, from->ref_null_value);
		align(column);
		ib_fprintf(out_file, "isc_ftof (%s, isc_%d, isc_%d, isc_%d);",
				   from->ref_value, blob->blb_len_ident,
				   blob->blb_buff_ident, blob->blb_len_ident);
	}
	else
		blob = (BLB) action->act_object;

	args.pat_blob = blob;
	args.pat_vector1 = status_vector(action);
	args.pat_condition = !(action->act_error
						   || (action->act_flags & ACT_sql));
	args.pat_ident1 = blob->blb_len_ident;
	args.pat_ident2 = blob->blb_buff_ident;
	args.pat_string1 = global_status_name;
	PATTERN_expand((USHORT) column, pattern1, &args);

	set_sqlcode(action, column);

	if (action->act_flags & ACT_sql)
		endp(column);
}


//____________________________________________________________
//  
//		Generate BLR/MBLR/etc. in raw, numeric form.  Ugly but dense.
//  

static void gen_raw(const UCHAR* blr, int request_length)
{
	TEXT buffer[80];

	TEXT* p = buffer;
	TEXT* limit = buffer + 60;

	for (int count = request_length; count; count--) {
		const TEXT c = *blr++;
		if ((c >= 'A' && c <= 'Z') || c == '$' || c == '_')
			sprintf(p, "'%c'", c);
		else
			sprintf(p, "%d", c);
		while (*p)
			p++;
		if (count - 1)
			*p++ = ',';
		if (p < limit)
			continue;
		*p = 0;
		printa(INDENT, buffer);
		p = buffer;
	}

	*p = 0;
	printa(INDENT, buffer);
}


//____________________________________________________________
//  
//		Generate substitution text for READY
//  

static void gen_ready( const act* action, int column)
{
	const TEXT* vector = status_vector(action);

	for (RDY ready = (RDY) action->act_object; ready; ready = ready->rdy_next) {
		const dbb* db = ready->rdy_database;
		TEXT* filename = ready->rdy_filename;
		if (!filename)
			filename = db->dbb_runtime;
		if ((action->act_error || (action->act_flags & ACT_sql)) &&
			ready != (RDY) action->act_object)
		{
			printa(column, "if (!%s [1]) {", global_status_name);
		}
		make_ready(db, filename, vector, (USHORT) column, ready->rdy_request);
		if ((action->act_error || (action->act_flags & ACT_sql)) &&
			ready != (RDY) action->act_object)
		{
			endp(column);
		}
	}
	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a send or receive call for a port.
//  

static void gen_receive( const act* action, int column, POR port)
{
	PAT args;
	const TEXT* pattern =
		"isc_receive (%V1, (FRBRD**) &%RH, (short) %PN, (short) %PL, &%PI, (short) %RL);";

	args.pat_request = action->act_request;
	args.pat_vector1 = status_vector(action);
	args.pat_port = port;
	PATTERN_expand((USHORT) column, pattern, &args);

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate substitution text for RELEASE_REQUESTS
//		For active databases, call isc_release_request.
//		for all others, just zero the handle.  For the
//		release request calls, ignore error returns, which
//		are likely if the request was compiled on a database
//		which has been released and re-readied.  If there is
//		a serious error, it will be caught on the next statement.
//  

static void gen_release( const act* action, int column)
{
	DBB exp_db = (DBB) action->act_object;

	for (GPRE_REQ request = requests; request; request = request->req_next) {
		DBB db = request->req_database;
		if (exp_db && db != exp_db)
			continue;
		if (db && request->req_handle && !(request->req_flags & REQ_exp_hand)) {
			printa(column, "if (%s && %s)",
				   db->dbb_name->sym_string, request->req_handle);
			printa(column + INDENT, "isc_release_request (%s, &%s);",
				   global_status_name, request->req_handle);
			printa(column, "%s = 0;", request->req_handle);
		}
	}
}


//____________________________________________________________
//  
//		Generate definitions associated with a single request.
//  

static void gen_request(const gpre_req* request)
{
	if (!(request->req_flags & (REQ_exp_hand | REQ_sql_blob_open 
		| REQ_sql_blob_create)) && request->req_type != REQ_slice
		&& request->req_type != REQ_procedure)
	{
		printa(0, "static isc_req_handle\n   %s = 0;\t\t/* request handle */\n",
			   request->req_handle);
	}

//  check the case where we need to extend the dpb dynamically at runtime,
//  in which case we need dpb length and a pointer to be defined even if 
//  there is no static dpb defined 

	if (request->req_flags & REQ_extend_dpb) {
		printa(0, "static char\n   *isc_%dp;", request->req_ident);
		if (!request->req_length)
			printa(0, "static short\n   isc_%dl = %d;", request->req_ident,
				   request->req_length);
	}

	if (request->req_type == REQ_create_database)
		printa(0, "static %s\n   *isc_%dt;", DCL_LONG, request->req_ident);

	if (request->req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
		printa(0,
			   "static isc_stmt_handle\n   isc_%ds;\t\t/* sql statement handle */",
			   request->req_ident);

	if (request->req_length) {
		if (request->req_flags & REQ_sql_cursor)
			printa(0,
				   "static isc_stmt_handle\n   isc_%ds;\t\t/* sql statement handle */",
				   request->req_ident);
#ifdef SCROLLABLE_CURSORS
		if (request->req_flags & REQ_scroll)
			printa(0,
				   "static short\n   isc_%ddirection;\t\t/* last direction sent to engine */",
				   request->req_ident);
#endif
		printa(0, "static %sshort\n   isc_%dl = %d;",
			   (request->req_flags & REQ_extend_dpb) ? "" : CONST_STR,
			   request->req_ident, request->req_length);
		printa(0, "static %schar\n   isc_%d [] = {",
			   CONST_STR, request->req_ident);
		const TEXT* string_type = "blr";
		if (sw_raw) {
			gen_raw(request->req_blr, request->req_length);

			switch (request->req_type) {
			case REQ_create_database:
			case REQ_ready:
				string_type = "dpb";
				break;

			case REQ_ddl:
				string_type = "dyn";
				break;
			case REQ_slice:
				string_type = "sdl";
				break;

			default:
				string_type = "blr";
			}
		}
		else
			switch (request->req_type) {
			case REQ_create_database:
			case REQ_ready:
				string_type = "dpb";
				if (PRETTY_print_cdb(request->req_blr, gen_blr, 0, 0))
				{
					CPR_error("internal error during parameter generation");
				}
				break;

			case REQ_ddl:
				string_type = "dyn";
				if (PRETTY_print_dyn(request->req_blr, gen_blr, 0, 0))
				{
					CPR_error("internal error during dynamic DDL generation");
				}
				break;
			case REQ_slice:
				string_type = "sdl";
				if (PRETTY_print_sdl(request->req_blr, gen_blr, 0, 0))
				{
					CPR_error("internal error during SDL generation");
				}
				break;

			default:
				string_type = "blr";
				if (gds__print_blr(request->req_blr, gen_blr, 0, 0))
				{
					CPR_error("internal error during BLR generation");
				}
			}
		printa(INDENT, "};\t/* end of %s string for request isc_%d */\n",
			   string_type, request->req_ident);
	}

//   Print out slice description language if there are arrays associated with request  

	for (const por* port = request->req_ports; port; port = port->por_next)
		for (const ref* reference = port->por_references; reference;
			 reference = reference->ref_next) 
		{
			if (reference->ref_sdl) {
				printa(0, "static %sshort\n   isc_%dl = %d;", CONST_STR,
					   reference->ref_sdl_ident, reference->ref_sdl_length);
				printa(0, "static %schar\n   isc_%d [] = {", CONST_STR,
					   reference->ref_sdl_ident);
				if (sw_raw)
					gen_raw((UCHAR *) reference->ref_sdl,
							reference->ref_sdl_length);
				else
					if (PRETTY_print_sdl(reference->ref_sdl, gen_blr, 0, 0))
					{
						CPR_error("internal error during SDL generation");
					}
				printa(INDENT,
					   "};\t/* end of sdl string for request isc_%d */\n",
					   reference->ref_sdl_ident);
			}
		}

//  Print out any blob parameter blocks required 

	for (const blb* blob = request->req_blobs; blob; blob = blob->blb_next)
		if (blob->blb_bpb_length) {
			printa(0, "static %schar\n   isc_%d [] = {", CONST_STR,
				   blob->blb_bpb_ident);
			gen_raw(blob->blb_bpb, blob->blb_bpb_length);
			printa(INDENT, "};\n");
		}
//  If this is a GET_SLICE/PUT_slice, allocate some variables 

	if (request->req_type == REQ_slice) {
		printa(0, "static %s", DCL_LONG);
		printa(INDENT, "isc_%dv [%d],", request->req_ident,
			   MAX(request->req_slice->slc_parameters, 1));
		printa(INDENT, "isc_%ds;", request->req_ident);
	}
}


//____________________________________________________________
//  
//		Generate receive call for a port
//		in a store2 statement.
//  

static void gen_return_value( const act* action, int column)
{
	GPRE_REQ request = action->act_request;
	if (action->act_pair->act_error)
		column += INDENT;
	align(column);
	gen_start(action, request->req_primary, column, true);
	UPD update = (UPD) action->act_object;
	REF reference = update->upd_references;
	gen_receive(action, column, reference->ref_port);
}


//____________________________________________________________
//  
//		Process routine head.  If there are requests in the
//		routine, insert local definitions.
//  

static void gen_routine( const act* action, int column)
{
	column += INDENT;

	for (GPRE_REQ request = (GPRE_REQ) action->act_object; request;
		 request = request->req_routine)
	{
		if (request->req_type == REQ_any)
			continue;
		for (POR port = request->req_ports; port; port = port->por_next)
			make_port(port, column);
		for (BLB blob = request->req_blobs; blob; blob = blob->blb_next) {
			printa(column, "isc_blob_handle\t\tisc_%d;\t\t/* blob handle */",
				   blob->blb_ident);
			printa(column, "char\t\t\tisc_%d [%d];\t/* blob segment */",
				   blob->blb_buff_ident, blob->blb_seg_length);
			printa(column, "unsigned short\tisc_%d;\t\t/* segment length */",
				   blob->blb_len_ident);
		}
	}
}


//____________________________________________________________
//  
//		Generate substitution text for END_STREAM.
//  

static void gen_s_end( const act* action, int column)
{
	if (action->act_error)
		begin(column);

	GPRE_REQ request = action->act_request;

	if (action->act_type == ACT_close)
		column = gen_cursor_close(action, request, column);

	printa(column, "isc_unwind_request (%s, &%s, %s);",
		   status_vector(action),
		   request->req_handle, request->req_request_level);

	if (action->act_type == ACT_close) {
		endp(column);
		column -= INDENT;
	}

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate substitution text for FETCH.
//  

static void gen_s_fetch( const act* action, int column)
{
	GPRE_REQ request = action->act_request;

	if (request->req_sync)
		gen_send(action, request->req_sync, column);

	gen_receive(action, column, request->req_primary);

	if (!action->act_pair && !action->act_error)
		endp(column);
}


//____________________________________________________________
//  
//		Generate text to compile and start a stream.  This is
//		used both by START_STREAM and FOR
//  

static void gen_s_start( const act* action, int column)
{
	GPRE_REQ request = action->act_request;

	gen_compile(action, column);

	POR port = request->req_vport;
	if (port)
		asgn_from(action, port->por_references, column);

	if (action->act_type == ACT_open)
		column = gen_cursor_open(action, request, column);

	if (action->act_error || (action->act_flags & ACT_sql)) {
		make_ok_test(action, request, column);
		column += INDENT;
	}

	gen_start(action, port, column, false);

	if (action->act_error || (action->act_flags & ACT_sql))
		column -= INDENT;

	if (action->act_type == ACT_open) {
		endp(column);
		column -= INDENT;
		endp(column);
		column -= INDENT;
	}

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Substitute for a segment, segment length, or blob handle.
//  

static void gen_segment( const act* action, int column)
{
	BLB blob = (BLB) action->act_object;

	printa(column, "isc_%d",
		   (action->act_type == ACT_segment) ? blob->blb_buff_ident :
		   (action->act_type == ACT_segment_length) ? blob->blb_len_ident :
		   blob->blb_ident);
}


//____________________________________________________________
//  
//  
//		generate code for a singleton select.
//  

static void gen_select( const act* action, int column)
{
	TEXT name[20];

	GPRE_REQ request = action->act_request;
	POR port = request->req_primary;
	gen_name(name, request->req_eof, true);

	gen_s_start(action, column);
	printa(column, "if (!SQLCODE) ");
	column += INDENT;
	begin(column);
	gen_receive(action, column, port);
	printa(column, "if (!SQLCODE)");
	column += INDENT;
	printa(column, "if (%s)", name);
	column += INDENT;

	begin(column);
	GPRE_NOD var_list = (GPRE_NOD) action->act_object;
	if (var_list)
		for (int i = 0; i < var_list->nod_count; i++) {
			align(column);
			asgn_to(action, (REF) var_list->nod_arg[i], column);
		}
	if (request->req_database->dbb_flags & DBB_v3) {
		gen_receive(action, column, port);
		printa(column, "if (!SQLCODE && %s)", name);
		printa(column + INDENT, "SQLCODE = -1;");
	}
	endp(column);

	printa(column - INDENT, "else");
	printa(column, "SQLCODE = 100;");
	column -= INDENT;
	column -= INDENT;
	endp(column);
}


//____________________________________________________________
//  
//		Generate a send or receive call for a port.
//  

static void gen_send( const act* action, POR port, int column)
{
	PAT args;
	const TEXT* pattern =
		"isc_send (%V1, (FRBRD**) &%RH, (short) %PN, (short) %PL, &%PI, (short) %RL);";

	args.pat_request = action->act_request;
	args.pat_vector1 = status_vector(action);
	args.pat_port = port;
	PATTERN_expand((USHORT) column, pattern, &args);

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate support for get/put slice statement.
//  

static void gen_slice( const act* action, REF var_reference, int column)
{
	PAT args;
	const TEXT* pattern1 = "isc_get_slice (%V1, &%DH, &%RT, &%FR, (short) %N1, \
(char *) %I1, (short) %N2, %I1v, %I1s, %S5, &isc_array_length);";
	const TEXT* pattern2 = "isc_put_slice (%V1, &%DH, &%RT, &%FR, (short) %N1, \
(char *) %I1, (short) %N2, %I1v, %I1s, %S5);";

	GPRE_REQ request = action->act_request;
	SLC slice = (SLC) action->act_object;
	GPRE_REQ parent_request = slice->slc_parent_request;

//  Compute array size 

	printa(column, "isc_%ds = %d", request->req_ident,
		   slice->slc_field->fld_array->fld_length);

	slc::slc_repeat *tail, *end;
	for (tail = slice->slc_rpt, end = tail + slice->slc_dimensions;
		 tail < end; ++tail)
	{
		if (tail->slc_upper != tail->slc_lower) {
			REF lower = (REF) tail->slc_lower->nod_arg[0];
			REF upper = (REF) tail->slc_upper->nod_arg[0];
			if (lower->ref_value)
				ib_fprintf(out_file, " * ( %s - %s + 1)", upper->ref_value,
						   lower->ref_value);
			else
				ib_fprintf(out_file, " * ( %s + 1)", upper->ref_value);
		}
	}

	ib_fprintf(out_file, ";");

//  Make assignments to variable vector 

	REF reference;
	for (reference = request->req_values; reference;
		reference = reference->ref_next)
	{
		printa(column, "isc_%dv [%d] = %s;",
				request->req_ident, reference->ref_id, reference->ref_value);
	}

	args.pat_reference =
		(var_reference ? var_reference : slice->slc_field_ref);
	args.pat_request = parent_request;	// blob id request
	args.pat_vector1 = status_vector(action);	// status vector
	args.pat_database = parent_request->req_database;	// database handle
	args.pat_string1 = action->act_request->req_trans;	// transaction handle
	args.pat_value1 = request->req_length;	// slice descr. length
	args.pat_ident1 = request->req_ident;	// request name
	args.pat_value2 = slice->slc_parameters * sizeof(SLONG);	// parameter length

	if (!(reference = var_reference))
		reference = (REF) slice->slc_array->nod_arg[0];
	args.pat_string5 = reference->ref_value;	// array name

	PATTERN_expand((USHORT) column,
				   (action->act_type == ACT_get_slice) ? pattern1 : pattern2,
				   &args);

	set_sqlcode(action, column);
	if (action->act_flags & ACT_sql)
		gen_whenever(action->act_whenever, column);
}


//____________________________________________________________
//  
//		Generate either a START or START_AND_SEND depending
//		on whether or a not a port is present.  If this START
//		or START_AND_SEND is being generated for a STORE or a
//		MODIFY statement, generate PUT_SLICE calls, as well.
//  

static void gen_start(const act* action,
					  POR port,
					  int column,
					  bool sending)
{
	PAT args;
	const TEXT* pattern1 =
		"isc_start_and_send (%V1, (FRBRD**) &%RH, (FRBRD**) &%S1, (short) %PN, (short) %PL, &%PI, (short) %RL);";
	const TEXT* pattern2 = "isc_start_request (%V1, (FRBRD**) &%RH, (FRBRD**) &%S1, (short) %RL);";
	REF reference;

	if (port && sending) {
		for (reference = port->por_references; reference;
			 reference = reference->ref_next)
		{
			if (reference->ref_field->fld_array_info)
					gen_get_or_put_slice(action, reference, false, column);
		}
	}

	args.pat_request = action->act_request;
	args.pat_vector1 = status_vector(action);
	args.pat_port = port;
	args.pat_string1 = request_trans(action, action->act_request);
	PATTERN_expand((USHORT) column, (port) ? pattern1 : pattern2, &args);
}


//____________________________________________________________
//  
//		Generate text for STORE statement.  This includes the compile
//		call and any variable initialization required.
//  

static void gen_store( const act* action, int column)
{
	TEXT name[64];

	GPRE_REQ request = action->act_request;
	align(column);
	gen_compile(action, column);

	if (action->act_error || (action->act_flags & ACT_sql)) {
		make_ok_test(action, request, column);
		column += INDENT;
		if (action->act_error)
			begin(column);
	}

//  Initialize any blob fields 

	POR port = request->req_primary;

	for (REF reference = port->por_references; reference;
		 reference = reference->ref_next)
	{
		GPRE_FLD field = reference->ref_field;
		if (field->fld_flags & FLD_blob)
			printa(column, "%s = isc_blob_null;",
				   gen_name(name, reference, true));
	}
}


//____________________________________________________________
//  
//		Generate substitution text for START_TRANSACTION.
//  

static void gen_t_start( const act* action, int column)
{
	const TEXT* vector = status_vector(action);

//  if this is a purely default transaction, just let it through 

	const gpre_tra* trans;
	if (!action || !(trans = (GPRE_TRA) action->act_object)) {
		t_start_auto(action, 0, vector, column, false);
		return;
	}

//  build a complete statement, including tpb's. Ready db's 
	const tpb* tpb_iterator;

	if (sw_auto)
		for (tpb_iterator = trans->tra_tpb;
			 tpb_iterator;
			 tpb_iterator = tpb_iterator->tpb_tra_next)
		{
			const dbb* db = tpb_iterator->tpb_database;
			TEXT* filename = db->dbb_runtime;
			if (filename || !(db->dbb_flags & DBB_sqlca)) {
				printa(column, "if (!%s)", db->dbb_name->sym_string);
				make_ready(db, filename, vector, (USHORT) (column + INDENT),
						   0);
			}
		}

	printa(column, "isc_start_transaction (%s, (FRBRD**) &%s, (short) %d",
		   vector,
		   (trans->tra_handle) ? trans->tra_handle : transaction_name,
		   trans->tra_db_count);

//  Some systems don't like infinitely long lines.  Limit them to 256. 

	int remaining = 256 - column - strlen(vector) -
		strlen((trans->tra_handle) ? trans->tra_handle : transaction_name) -
		31;

	for (tpb_iterator = trans->tra_tpb;
		 tpb_iterator;
		 tpb_iterator = tpb_iterator->tpb_tra_next)
	{
		int length = strlen(tpb_iterator->tpb_database->dbb_name->sym_string)
			+ 22;
		if (length > remaining) {
			align(column + INDENT);
			remaining = 256 - column - INDENT;
		}
		remaining -= length;
		ib_fprintf(out_file, ", &%s, (short) %d, isc_tpb_%d",
				   tpb_iterator->tpb_database->dbb_name->sym_string,
				   tpb_iterator->tpb_length, tpb_iterator->tpb_ident);
	}

	ib_fprintf(out_file, ");");

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Generate a TPB in the output file
//  

static void gen_tpb(tpb* tpb_buffer, int column)
{
	TEXT buffer[80];
	SSHORT length;

	printa(column, "static %schar\n", CONST_STR);
	column += INDENT;
	TEXT* p = buffer;

	for (length = 0; length < column; length++)
		*p++ = ' ';

	sprintf(p, "isc_tpb_%d [%d] = {", tpb_buffer->tpb_ident, tpb_buffer->tpb_length);
	while (*p)
		p++;

	SSHORT tpb_length = tpb_buffer->tpb_length;
	TEXT* text = (TEXT*) tpb_buffer->tpb_string;

	while (--tpb_length >= 0) {
		const TEXT c = *text++;
		if ((c >= 'A' && c <= 'Z') || c == '$' || c == '_')
			sprintf(p, "'%c'", c);
		else
			sprintf(p, "%d", c);
		while (*p)
			p++;
		if (tpb_length)
			*p++ = ',';
		if (p - buffer > 60) {
			*p = 0;
			ib_fprintf(out_file, " %s\n", buffer);
			p = buffer;
			for (length = 0; length < column + INDENT; length++)
				*p++ = ' ';
			*p = 0;
		}
	}

	ib_fprintf(out_file, "%s};\n", buffer);
}


//____________________________________________________________
//  
//		Generate substitution text for COMMIT, ROLLBACK, PREPARE, and SAVE
//  

static void gen_trans( const act* action, int column)
{

	if (action->act_type == ACT_commit_retain_context)
		printa(column, "isc_commit_retaining (%s, (FRBRD**) &%s);",
			   status_vector(action),
			   (action->act_object) ? (TEXT* ) (action->
												act_object) :
			   transaction_name);
	else
		printa(column, "isc_%s_transaction (%s, (FRBRD**) &%s);",
			   (action->act_type ==
				ACT_commit) ? "commit" : (action->act_type ==
										  ACT_rollback) ? "rollback" :
			   "prepare", status_vector(action),
			   (action->act_object) ? (TEXT* ) (action->
												act_object) :
			   transaction_name);

	set_sqlcode(action, column);
}


//____________________________________________________________
//  
//		Substitute for a variable reference.
//  

static void gen_type( const act* action, int column)
{

	printa(column, "%ld", (SLONG) action->act_object);
}


//____________________________________________________________
//  
//		Generate substitution text for UPDATE ... WHERE CURRENT OF ...
//  

static void gen_update( const act* action, int column)
{
	UPD modify = (UPD) action->act_object;
	POR port = modify->upd_port;
	asgn_from(action, port->por_references, column);
	gen_send(action, port, column);
}


//____________________________________________________________
//  
//		Substitute for a variable reference.
//  

static void gen_variable( const act* action, int column)
{
	TEXT s[20];

	printa(column, gen_name(s, action->act_object, false));
}


//____________________________________________________________
//  
//		Generate tests for any WHENEVER clauses that may have been declared.
//  

static void gen_whenever( const swe* label, int column)
{
	const TEXT* condition;

	while (label) {
		switch (label->swe_condition) {
		case SWE_error:
			condition = "SQLCODE < 0";
			break;

		case SWE_warning:
			condition = "SQLCODE > 0 && SQLCODE != 100";
			break;

		case SWE_not_found:
			condition = "SQLCODE == 100";
			break;
		}
		align(column);
		ib_fprintf(out_file, "if (%s) goto %s;", condition, label->swe_label);
		label = label->swe_next;
	}
}


//____________________________________________________________
//  
//		Generate a declaration of an array in the
//		output file.
//  

static void make_array_declaration(ref* reference)
{

	gpre_fld* field = reference->ref_field;
	const TEXT* name = field->fld_symbol->sym_string;
	TEXT s[64];

//  Don't generate multiple declarations for the array.  V3 Bug 569.  

	if (field->fld_array_info->ary_declared)
		return;

	field->fld_array_info->ary_declared = true;
	const TEXT* dtype;

	switch (field->fld_array_info->ary_dtype) {
	case dtype_short:
		dtype = "short";
		break;

	case dtype_long:
		dtype = DCL_LONG;
		break;

	case dtype_cstring:
	case dtype_text:
	case dtype_varying:
		dtype = "char ";
		break;

	case dtype_quad:
		dtype = DCL_QUAD;
		break;

// ** Begin date/time/timestamp *
	case dtype_sql_date:
		dtype = "ISC_DATE";
		break;

	case dtype_sql_time:
		dtype = "ISC_TIME";
		break;

	case dtype_timestamp:
		dtype = "ISC_TIMESTAMP";
		break;
// ** End date/time/timestamp *

	case dtype_int64:
		dtype = "ISC_INT64";
		break;

	case dtype_real:
		dtype = "float ";
		break;

	case dtype_double:
		dtype = "double";
		break;

	default:
		sprintf(s, "datatype %d unknown for field %s",
				field->fld_array_info->ary_dtype, name);
		CPR_error(s);
		return;
	}

	ib_fprintf(out_file, "static %s isc_%d", dtype,
			   field->fld_array_info->ary_ident);

//   Print out the dimension part of the declaration  

	for (DIM dimension = field->fld_array_info->ary_dimension; dimension;
		 dimension = dimension->dim_next)
	{
		ib_fprintf(out_file, " [%ld]",
				   dimension->dim_upper - dimension->dim_lower + 1);
	}

	if (field->fld_array_info->ary_dtype <= dtype_varying)
		ib_fprintf(out_file, " [%d]", field->fld_array->fld_length);

//   Print out the database field  

	ib_fprintf(out_file, ";\t/* %s */\n", name);
}


//____________________________________________________________
//  
//		Turn a symbol into a varying string.
//  
// CVC: this code in unclear to me: it's advancing sym_string pointer,
// so after this call the pointer is at the position of the null terminator.

static TEXT* make_name( TEXT* string, sym* symbol)
{

	if (symbol->sym_type == SYM_delimited_cursor) {
		// All This elaborate code is just to put double quotes around
		// the cursor names and to escape any embeded quotes.
		int i = 0;
		strcpy(string, "\"\\\"");
		for (i = strlen(string); *symbol->sym_string; i++) {
			if (*symbol->sym_string == '\"' || *symbol->sym_string == '\'') {
				string[i++] = '\\';
				string[i++] = *symbol->sym_string;
				string[i++] = '\\';
			}
			string[i] = *symbol->sym_string++;
		}
		string[i] = 0;
		strcat(string, "\\\"\"");
	}
	else
		sprintf(string, "\"%s\"", symbol->sym_string);

	return string;
}


//____________________________________________________________
//  
//		Generate code to test existence of compiled request with
//		active transaction
//  

static void make_ok_test( const act* action, const gpre_req* request, int column)
{

	if (sw_auto)
		printa(column, "if (%s && %s)",
			   request_trans(action, request), request->req_handle);
	else
		printa(column, "if (%s)", request->req_handle);
}


//____________________________________________________________
//  
//		Insert a port record description in output.
//  

static void make_port(const por* port, int column)
{
	printa(column, "struct {");

	for (const ref* reference = port->por_references; reference;
		 reference = reference->ref_next)
	{
		align(column + INDENT);
		const gpre_fld* field = reference->ref_field;
		const TEXT* name;
		const sym* symbol = field->fld_symbol;
		if (symbol)
			name = symbol->sym_string;
		else
			name = "<expression>";
		if (reference->ref_value && (reference->ref_flags & REF_array_elem))
			field = field->fld_array;
		int fld_len = 0;
		const TEXT* dtype;

		switch (field->fld_dtype) {
		case dtype_short:
			dtype = "short";
			break;

		case dtype_long:
			dtype = DCL_LONG;
			break;

		case dtype_cstring:
		case dtype_text:
			dtype = "char ";
			if (field->fld_sub_type != 1 || field->fld_length > 1)
				fld_len = field->fld_length;
			break;

		case dtype_quad:
			dtype = DCL_QUAD;
			break;

// ** Begin date/time/timestamp *
		case dtype_sql_date:
			dtype = "ISC_DATE";
			break;

		case dtype_sql_time:
			dtype = "ISC_TIME";
			break;

		case dtype_timestamp:
			dtype = "ISC_TIMESTAMP";
			break;

		case dtype_int64:
			dtype = "ISC_INT64";
			break;

		case dtype_blob:
			dtype = "ISC_QUAD";
			break;

		case dtype_real:
			dtype = "float ";
			break;

		case dtype_double:
			dtype = "double";
			break;

		default:
			{
				TEXT s[80];
				sprintf(s, "datatype %d unknown for field %s, msg %d",
						field->fld_dtype, name, port->por_msg_number);
				CPR_error(s);
				return;
			}
		}
		if (fld_len)
			ib_fprintf(out_file, "    %s isc_%d [%d];\t/* %s */",
					   dtype, reference->ref_ident, fld_len, name);
		else
			ib_fprintf(out_file, "    %s isc_%d;\t/* %s */",
					   dtype, reference->ref_ident, name);
	}

	printa(column, "} isc_%d;", port->por_ident);
}


//____________________________________________________________
//  
//		Generate the actual insertion text for a
//		ready;
//  

static void make_ready(
					   const dbb* db,
					   const TEXT* filename,
					   const TEXT* vector, USHORT column,
					   const gpre_req* request)
{
	TEXT s1[32], s2[32];

	if (request) {
		sprintf(s1, "isc_%dl", request->req_ident);

		if (request->req_flags & REQ_extend_dpb)
			sprintf(s2, "isc_%dp", request->req_ident);
		else
			sprintf(s2, "isc_%d", request->req_ident);

		// if the dpb needs to be extended at runtime to include items
		// in host variables, do so here; this assumes that there is
		// always a request generated for runtime variables

		if (request->req_flags & REQ_extend_dpb) {
			if (request->req_length)
				printa(column, "%s = isc_%d;", s2, request->req_ident);
			else
				printa(column, "%s = (char*) 0;", s2);

			printa(column,
				   "isc_expand_dpb (&%s, &%s, isc_dpb_user_name, %s, isc_dpb_password, %s, isc_dpb_sql_role_name, %s, isc_dpb_lc_messages, %s, isc_dpb_lc_ctype, %s, 0);",
				   s2, s1, db->dbb_r_user ? db->dbb_r_user : "(char*) 0",
				   db->dbb_r_password ? db->dbb_r_password : "(char*) 0",
				   db->dbb_r_sql_role ? db->dbb_r_sql_role : "(char*) 0",
				   db->dbb_r_lc_messages ? db->
				   dbb_r_lc_messages : "(char*) 0",
				   db->dbb_r_lc_ctype ? db->dbb_r_lc_ctype : "(char*) 0");
		}
	}

//  generate the attach database itself 

	const TEXT* dpb_size_ptr = "0";
	const TEXT* dpb_ptr = "(char*) 0";

	align(column);
	if (filename)
		ib_fprintf(out_file,
				   "isc_attach_database (%s, 0, %s, &%s, %s, %s);",
				   vector,
				   filename,
				   db->dbb_name->sym_string,
				   (request ? s1 : dpb_size_ptr), (request ? s2 : dpb_ptr));
	else
		ib_fprintf(out_file,
				   "isc_attach_database (%s, 0, \"%s\", &%s, %s, %s);",
				   vector,
				   db->dbb_filename,
				   db->dbb_name->sym_string,
				   (request ? s1 : dpb_size_ptr), (request ? s2 : dpb_ptr));

//  if the dpb was extended, free it here 

	if (request && request->req_flags & REQ_extend_dpb) {
		if (request->req_length)
			printa(column, "if (%s != isc_%d)", s2, request->req_ident);
		printa(column + (request->req_length ? INDENT : 0), "isc_free ((char*) %s);",
			   s2);

		// reset the length of the dpb

		printa(column, "%s = %d;", s1, request->req_length);
	}
}


//____________________________________________________________
//  
//		Print a fixed string at a particular column.
//  

static void printa( int column, const char* string, ...)
{
	va_list ptr;

	VA_START(ptr, string);
	align(column);
	ib_vfprintf(out_file, string, ptr);
}


//____________________________________________________________
//  
//		Print a fixed string at a particular column.
//  

static void printb( const TEXT* string, ...)
{
	va_list ptr;

	VA_START(ptr, string);
	ib_vfprintf(out_file, string, ptr);
}


//____________________________________________________________
//  
//		Generate the appropriate transaction handle.
//  

static const TEXT* request_trans( const act* action, const gpre_req* request)
{

	if (action->act_type == ACT_open) {
		const TEXT* trname = ((OPN) action->act_object)->opn_trans;
		if (!trname) {
			trname = transaction_name;
		}
		return trname;
	}

	return (request) ? request->req_trans : transaction_name;
}


//____________________________________________________________
//  
//		Generate the appropriate status vector parameter for a gds
//		call depending on where or not the action has an error clause.
//  

static const TEXT* status_vector( const act* action)
{

	if (action && (action->act_error || (action->act_flags & ACT_sql)))
		return global_status_name;

	return NULL_STATUS;
}


//____________________________________________________________
//  
//		Generate substitution text for START_TRANSACTION.
//		The complications include the fact that all databases
//		must be readied, and that everything should stop if
//		any thing fails so we don't trash the status vector.
//  

static void t_start_auto(const act* action,
						 const gpre_req* request,
						 const TEXT* vector,
						 int column,
						 bool test)
{
	const TEXT* trname = request_trans(action, request);

//  find out whether we're using a status vector or not 

	const int stat = !strcmp(vector, global_status_name);

//  this is a default transaction, make sure all databases are ready 

	begin(column);

	const dbb* db;
	int count;
	
	if (sw_auto) {
	    TEXT buffer[256];
		buffer[0] = 0;
		for (count = 0, db = isc_databases; db; db = db->dbb_next, count++)
		{
		    TEXT* filename = db->dbb_runtime;
			if (filename || !(db->dbb_flags & DBB_sqlca)) {
				align(column);
				ib_fprintf(out_file, "if (!%s", db->dbb_name->sym_string);
				if (stat && buffer[0])
					ib_fprintf(out_file, " && !%s [1]", vector);
				ib_fprintf(out_file, ")");
				make_ready(db, filename, vector, (USHORT) (column + INDENT),
						   0);
				if (buffer[0])
					strcat(buffer, " && ");
				strcat(buffer, db->dbb_name->sym_string);
			}
		}
		if (!buffer[0])
			strcpy(buffer, "1");
		if (test)
			printa(column, "if (%s && !%s)", buffer, trname);
		else
			printa(column, "if (%s)", buffer);
		column += INDENT;
	}
	else
		for (count = 0, db = isc_databases; db; db = db->dbb_next, count++);

	printa(column, "isc_start_transaction (%s, (FRBRD**) &%s, (short) %d",
		   vector, trname, count);

//  Some systems don't like infinitely long lines.  Limit them to 256. 

	int remaining = 256 - column - strlen(vector) - strlen(trname) - 31;

	for (db = isc_databases; db; db = db->dbb_next) {
	    int length = strlen(db->dbb_name->sym_string) + 17;
		if (length > remaining) {
			align(column + INDENT);
			remaining = 256 - column - INDENT;
		}
		remaining -= length;
		ib_fprintf(out_file, ", &%s, (short) 0, (char*) 0",
				   db->dbb_name->sym_string);
	}

	ib_fprintf(out_file, ");");

	if (sw_auto)
		column -= INDENT;

	endp(column);
}


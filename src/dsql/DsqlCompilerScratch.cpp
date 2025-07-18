/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "../dsql/DsqlCompilerScratch.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/jrd.h"
#include "firebird/impl/blr.h"
#include "../jrd/RecordSourceNodes.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include <unordered_set>

using namespace Firebird;
using namespace Jrd;


#ifdef DSQL_DEBUG
void DsqlCompilerScratch::dumpContextStack(const DsqlContextStack* stack)
{
	printf("Context dump\n");
	printf("------------\n");

	for (DsqlContextStack::const_iterator i(*stack); i.hasData(); ++i)
	{
		const dsql_ctx* context = i.object();

		printf("scope: %2d; number: %2d; system: %d; returning: %d; alias: %-*.*s; "
			   "internal alias: %-*.*s\n",
			context->ctx_scope_level,
			context->ctx_context,
			(context->ctx_flags & CTX_system) != 0,
			(context->ctx_flags & CTX_returning) != 0,
			MAX_SQL_IDENTIFIER_SIZE, MAX_SQL_IDENTIFIER_SIZE, context->ctx_alias[0].toQuotedString().c_str(),
			MAX_SQL_IDENTIFIER_SIZE, MAX_SQL_IDENTIFIER_SIZE, context->ctx_internal_alias.toQuotedString().c_str());
	}
}
#endif


void DsqlCompilerScratch::qualifyNewName(QualifiedName& name) const
{
	const auto tdbb = JRD_get_thread_data();

	if (!dbb->dbb_attachment->qualifyNewName(tdbb, name))
	{
		if (name.schema.isEmpty())
			status_exception::raise(Arg::Gds(isc_dyn_cannot_infer_schema));
		else
			status_exception::raise(Arg::Gds(isc_dyn_schema_not_found) << name.schema.toQuotedString());
	}
}

void DsqlCompilerScratch::qualifyExistingName(QualifiedName& name, std::initializer_list<ObjectType> objectTypes)
{
	if (!(name.schema.isEmpty() && name.object.hasData()))
		return;

	const auto tdbb = JRD_get_thread_data();
	const auto attachment = tdbb->getAttachment();

	if (ddlSchema.hasData())
	{
		if (!cachedDdlSchemaSearchPath)
		{
			cachedDdlSchemaSearchPath = FB_NEW_POOL(getPool()) ObjectsArray<MetaString>(getPool(), {ddlSchema});

			if (const auto& searchPath = *attachment->att_schema_search_path;
				std::find(searchPath.begin(), searchPath.end(), SYSTEM_SCHEMA) != searchPath.end())
			{
				cachedDdlSchemaSearchPath->push(SYSTEM_SCHEMA);
			}
		}

		attachment->qualifyExistingName(tdbb, name, objectTypes, cachedDdlSchemaSearchPath);
	}
	else
		attachment->qualifyExistingName(tdbb, name, objectTypes);
}


std::variant<std::monostate, dsql_prc*, dsql_rel*, dsql_udf*> DsqlCompilerScratch::resolveRoutineOrRelation(
	QualifiedName& name, std::initializer_list<ObjectType> objectTypes)
{
	const std::unordered_set<ObjectType> objectTypesSet(objectTypes);
	const bool searchProcedures = objectTypesSet.find(obj_procedure) != objectTypesSet.end();
	const bool searchRelations = objectTypesSet.find(obj_relation) != objectTypesSet.end();
	const bool searchFunctions = objectTypesSet.find(obj_udf) != objectTypesSet.end();
	fb_assert((searchProcedures || searchRelations) != searchFunctions);

	std::variant<std::monostate, dsql_prc*, dsql_rel*, dsql_udf*> object;

	const auto notFound = [&]()
	{
		return std::holds_alternative<std::monostate>(object);
	};

	const auto setObject = [&](const auto value)
	{
		if (value)
			object = value;
	};

	// search subroutine: name
	if (name.schema.isEmpty() &&
		name.package.isEmpty())
	{
		if (searchProcedures)
		{
			if (const auto subProcedure = getSubProcedure(name.object))
				setObject(subProcedure->dsqlProcedure);
		}

		if (searchFunctions)
		{
			if (const auto subFunction = getSubFunction(name.object))
				setObject(subFunction->dsqlFunction);
		}
	}

	// search packaged routine in the same package: name, same_package.name
	if (notFound() &&
		package.object.hasData() &&
		name.package.isEmpty() &&
		(name.schema.isEmpty() ||
			(!name.isUnambiguous() && name.schema == package.object)))
	{
		const QualifiedName routineName(name.object, package.schema, package.object);

		if (searchProcedures)
			setObject(METD_get_procedure(getTransaction(), this, routineName));

		if (searchFunctions)
			setObject(METD_get_function(getTransaction(), this, routineName));
	}

	// search standalone routine or relation: name, name1%schema.name2, name1.name2
	if (notFound() &&
		name.package.isEmpty())
	{
		auto qualifiedName = name;
		qualifyExistingName(qualifiedName, objectTypes);

		if (searchProcedures)
			setObject(METD_get_procedure(getTransaction(), this, qualifiedName));

		if (searchRelations)
			setObject(METD_get_relation(getTransaction(), this, qualifiedName));

		if (searchFunctions)
			setObject(METD_get_function(getTransaction(), this, qualifiedName));
	}

	// search packaged routine: name1%package.name2, name1.name2.name3
	if (notFound() &&
		name.package.hasData())
	{
		auto qualifiedName = name;
		qualifyExistingName(qualifiedName, objectTypes);

		if (searchProcedures)
			setObject(METD_get_procedure(getTransaction(), this, qualifiedName));

		if (searchFunctions)
			setObject(METD_get_function(getTransaction(), this, qualifiedName));
	}

	// search packaged routine: name1.name2
	if (notFound() &&
		!name.isUnambiguous() &&
		name.schema.hasData() &&
		name.package.isEmpty())
	{
		QualifiedName qualifiedName(name.object, {}, name.schema);
		qualifyExistingName(qualifiedName, objectTypes);

		if (searchProcedures)
			setObject(METD_get_procedure(getTransaction(), this, qualifiedName));

		if (searchFunctions)
			setObject(METD_get_function(getTransaction(), this, qualifiedName));
	}

	if (const auto procedure = std::get_if<dsql_prc*>(&object))
		name = (*procedure)->prc_name;
	else if (const auto relation = std::get_if<dsql_rel*>(&object))
		name = (*relation)->rel_name;
	else if (const auto function = std::get_if<dsql_udf*>(&object))
		name = (*function)->udf_name;

	return object;
}


void DsqlCompilerScratch::putBlrMarkers(ULONG marks)
{
	appendUChar(blr_marks);
	if (marks <= MAX_UCHAR)
	{
		appendUChar(1);
		appendUChar(marks);
	}
	else if (marks <= MAX_USHORT)
	{
		appendUChar(2);
		appendUShort(marks);
	}
	else
	{
		appendUChar(4);
		appendULong(marks);
	}
}


// Write out field data type.
// Taking special care to declare international text.
void DsqlCompilerScratch::putDtype(const TypeClause* field, bool useSubType)
{
#ifdef DEV_BUILD
	// Check if the field describes a known datatype

	if (field->dtype > FB_NELEM(blr_dtypes) || !blr_dtypes[field->dtype])
	{
		SCHAR buffer[100];
		snprintf(buffer, sizeof(buffer), "Invalid dtype %d in BlockNode::putDtype", field->dtype);
		ERRD_bugcheck(buffer);
	}
#endif

	if (field->notNull)
		appendUChar(blr_not_nullable);

	if (field->typeOfName.object.hasData())
	{
		if (field->typeOfTable.object.hasData())
		{
			if (field->explicitCollation)
			{
				if (field->typeOfTable.schema != ddlSchema)
				{
					appendUChar(blr_column_name3);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfTable.schema.c_str());
					appendMetaString(field->typeOfTable.object.c_str());
					appendMetaString(field->typeOfName.object.c_str());
					appendUChar(1);
					appendUShort(field->textType);
				}
				else
				{
					appendUChar(blr_column_name2);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfTable.object.c_str());
					appendMetaString(field->typeOfName.object.c_str());
					appendUShort(field->textType);
				}
			}
			else
			{
				if (field->typeOfTable.schema != ddlSchema)
				{
					appendUChar(blr_column_name3);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfTable.schema.c_str());
					appendMetaString(field->typeOfTable.object.c_str());
					appendMetaString(field->typeOfName.object.c_str());
					appendUChar(0);
				}
				else
				{
					appendUChar(blr_column_name);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfTable.object.c_str());
					appendMetaString(field->typeOfName.object.c_str());
				}
			}
		}
		else
		{
			if (field->explicitCollation)
			{
				if (field->typeOfName.schema != ddlSchema)
				{
					appendUChar(blr_domain_name3);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfName.schema.c_str());
					appendMetaString(field->typeOfName.object.c_str());
					appendUChar(1);
					appendUShort(field->textType);
				}
				else
				{
					appendUChar(blr_domain_name2);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfName.object.c_str());
					appendUShort(field->textType);
				}
			}
			else
			{
				if (field->typeOfName.schema != ddlSchema)
				{
					appendUChar(blr_domain_name3);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfName.schema.c_str());
					appendMetaString(field->typeOfName.object.c_str());
					appendUChar(0);
				}
				else
				{
					appendUChar(blr_domain_name);
					appendUChar(field->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(field->typeOfName.object.c_str());
				}
			}
		}

		return;
	}

	switch (field->dtype)
	{
		case dtype_cstring:
		case dtype_text:
		case dtype_varying:
		case dtype_blob:
			if (!useSubType)
				appendUChar(blr_dtypes[field->dtype]);
			else if (field->dtype == dtype_varying)
			{
				appendUChar(blr_varying2);
				appendUShort(field->textType);
			}
			else if (field->dtype == dtype_cstring)
			{
				appendUChar(blr_cstring2);
				appendUShort(field->textType);
			}
			else if (field->dtype == dtype_blob)
			{
				appendUChar(blr_blob2);
				appendUShort(field->subType);
				appendUShort(field->textType);
			}
			else
			{
				appendUChar(blr_text2);
				appendUShort(field->textType);
			}

			if (field->dtype == dtype_varying)
				appendUShort(field->length - sizeof(USHORT));
			else if (field->dtype != dtype_blob)
				appendUShort(field->length);
			break;

		default:
			appendUChar(blr_dtypes[field->dtype]);
			if (DTYPE_IS_EXACT(field->dtype) || (dtype_quad == field->dtype))
				appendUChar(field->scale);
			break;
	}
}

void DsqlCompilerScratch::putType(const TypeClause* type, bool useSubType)
{
#ifdef DEV_BUILD
	// Check if the field describes a known datatype
	if (type->dtype > FB_NELEM(blr_dtypes) || !blr_dtypes[type->dtype])
	{
		SCHAR buffer[100];
		snprintf(buffer, sizeof(buffer), "Invalid dtype %d in put_dtype", type->dtype);
		ERRD_bugcheck(buffer);
	}
#endif

	if (type->notNull)
		appendUChar(blr_not_nullable);

	if (type->typeOfName.object.hasData())
	{
		if (type->typeOfTable.object.hasData())
		{
			if (type->collate.object.hasData())
			{
				if (type->typeOfTable.schema != ddlSchema)
				{
					appendUChar(blr_column_name3);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfTable.schema.c_str());
					appendMetaString(type->typeOfTable.object.c_str());
					appendMetaString(type->typeOfName.object.c_str());
					appendUChar(1);
					appendUShort(type->textType);
				}
				else
				{
					appendUChar(blr_column_name2);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfTable.object.c_str());
					appendMetaString(type->typeOfName.object.c_str());
					appendUShort(type->textType);
				}
			}
			else
			{
				if (type->typeOfTable.schema != ddlSchema)
				{
					appendUChar(blr_column_name3);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfTable.schema.c_str());
					appendMetaString(type->typeOfTable.object.c_str());
					appendMetaString(type->typeOfName.object.c_str());
					appendUChar(0);
				}
				else
				{
					appendUChar(blr_column_name);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfTable.object.c_str());
					appendMetaString(type->typeOfName.object.c_str());
				}
			}
		}
		else
		{
			if (type->collate.object.hasData())
			{
				if (type->typeOfName.schema != ddlSchema)
				{
					appendUChar(blr_domain_name3);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfName.schema.c_str());
					appendMetaString(type->typeOfName.object.c_str());
					appendUChar(1);
					appendUShort(type->textType);
				}
				else
				{
					appendUChar(blr_domain_name2);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfName.object.c_str());
					appendUShort(type->textType);
				}
			}
			else
			{
				if (type->typeOfName.schema != ddlSchema)
				{
					appendUChar(blr_domain_name3);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfName.schema.c_str());
					appendMetaString(type->typeOfName.object.c_str());
					appendUChar(0);
				}
				else
				{
					appendUChar(blr_domain_name);
					appendUChar(type->fullDomain ? blr_domain_full : blr_domain_type_of);
					appendMetaString(type->typeOfName.object.c_str());
				}
			}
		}

		return;
	}

	switch (type->dtype)
	{
		case dtype_cstring:
		case dtype_text:
		case dtype_varying:
		case dtype_blob:
			if (!useSubType)
				appendUChar(blr_dtypes[type->dtype]);
			else if (type->dtype == dtype_varying)
			{
				appendUChar(blr_varying2);
				appendUShort(type->textType);
			}
			else if (type->dtype == dtype_cstring)
			{
				appendUChar(blr_cstring2);
				appendUShort(type->textType);
			}
			else if (type->dtype == dtype_blob)
			{
				appendUChar(blr_blob2);
				appendUShort(type->subType);
				appendUShort(type->textType);
			}
			else
			{
				appendUChar(blr_text2);
				appendUShort(type->textType);
			}

			if (type->dtype == dtype_varying)
				appendUShort(type->length - sizeof(USHORT));
			else if (type->dtype != dtype_blob)
				appendUShort(type->length);
			break;

		default:
			appendUChar(blr_dtypes[type->dtype]);
			if (DTYPE_IS_EXACT(type->dtype) || dtype_quad == type->dtype)
				appendUChar(type->scale);
			break;
	}
}

// Write out local variable field data type.
void DsqlCompilerScratch::putLocalVariableDecl(dsql_var* variable, DeclareVariableNode* hostParam,
	QualifiedName& collationName)
{
	const auto field = variable->field;

	appendUChar(blr_dcl_variable);
	appendUShort(variable->number);
	DDL_resolve_intl_type(this, field, collationName);

	putDtype(field, true);

	if (variable->field->fld_name.hasData())	// Not a function return value
		putDebugVariable(variable->number, variable->field->fld_name);

	if (variable->type != dsql_var::TYPE_INPUT && hostParam && hostParam->dsqlDef->defaultClause)
	{
		hostParam->dsqlDef->defaultClause->value =
			Node::doDsqlPass(this, hostParam->dsqlDef->defaultClause->value, true);
	}

	variable->initialized = true;
}

// Write out local variable initialization.
void DsqlCompilerScratch::putLocalVariableInit(dsql_var* variable, const DeclareVariableNode* hostParam)
{
	const dsql_fld* field = variable->field;

	// Check for a default value, borrowed from define_domain
	NestConst<ValueSourceClause> node = hostParam ? hostParam->dsqlDef->defaultClause : nullptr;

	if (variable->type == dsql_var::TYPE_INPUT)
	{
		// Assign EXECUTE BLOCK's input parameter to its corresponding internal variable.

		appendUChar(blr_assignment);

		appendUChar(blr_parameter2);
		appendUChar(variable->msgNumber);
		appendUShort(variable->msgItem);
		appendUShort(variable->msgItem + 1);

		appendUChar(blr_variable);
		appendUShort(variable->number);
	}
	else if (node || (!field->fullDomain && !field->notNull))
	{
		appendUChar(blr_assignment);

		if (node)
			GEN_expr(this, node->value);
		else
			appendUChar(blr_null);	// Initialize variable to NULL

		appendUChar(blr_variable);
		appendUShort(variable->number);
	}
	else
	{
		appendUChar(blr_init_variable);
		appendUShort(variable->number);
	}
}

// Put maps in subroutines for outer variables/parameters usage.
void DsqlCompilerScratch::putOuterMaps()
{
	if (!outerMessagesMap.count() && !outerVarsMap.count())
		return;

	appendUChar(blr_outer_map);

	for (auto& [inner, outer] : outerVarsMap)
	{
		appendUChar(blr_outer_map_variable);
		appendUShort(inner);
		appendUShort(outer);
	}

	for (auto& [inner, outer] : outerMessagesMap)
	{
		appendUChar(blr_outer_map_message);
		appendUShort(inner);
		appendUShort(outer);
	}

	appendUChar(blr_end);
}

// Make a variable.
dsql_var* DsqlCompilerScratch::makeVariable(dsql_fld* field, const char* name,
	const dsql_var::Type type, USHORT msgNumber, USHORT itemNumber, std::optional<USHORT> localNumber)
{
	DEV_BLKCHK(field, dsql_type_fld);

	MemoryPool& pool = getPool();

	dsql_var* dsqlVar = FB_NEW_POOL(pool) dsql_var(pool);
	dsqlVar->type = type;
	dsqlVar->msgNumber = msgNumber;
	dsqlVar->msgItem = itemNumber;
	dsqlVar->number = localNumber.has_value() ? localNumber.value() : nextVarNumber++;
	dsqlVar->field = field;

	if (field)
		DsqlDescMaker::fromField(&dsqlVar->desc, field);

	if (type == dsql_var::TYPE_HIDDEN)
		hiddenVariables.push(dsqlVar);
	else
	{
		variables.push(dsqlVar);

		if (type == dsql_var::TYPE_OUTPUT)
			outputVariables.push(dsqlVar);
	}

	return dsqlVar;
}

// Try to resolve variable name against parameters and local variables.
dsql_var* DsqlCompilerScratch::resolveVariable(const MetaName& varName)
{
	for (dsql_var* const* i = variables.begin(); i != variables.end(); ++i)
	{
		const dsql_var* variable = *i;

		if (variable->field->fld_name == varName.c_str())
			return *i;
	}

	return NULL;
}

// Generate BLR for a return.
void DsqlCompilerScratch::genReturn(bool eosFlag)
{
	const bool hasEos = !(flags & (FLAG_TRIGGER | FLAG_FUNCTION | FLAG_EXEC_BLOCK));

	appendUChar(blr_send);
	appendUChar(1);
	appendUChar(blr_begin);

	for (Array<dsql_var*>::const_iterator i = outputVariables.begin(); i != outputVariables.end(); ++i)
	{
		const dsql_var* variable = *i;
		appendUChar(blr_assignment);
		appendUChar(blr_variable);
		appendUShort(variable->number);
		appendUChar(blr_parameter2);
		appendUChar(variable->msgNumber);
		appendUShort(variable->msgItem);
		appendUShort(variable->msgItem + 1);
	}

	if (hasEos)
	{
		appendUChar(blr_assignment);
		appendUChar(blr_literal);
		appendUChar(blr_short);
		appendUChar(0);
		appendUShort((eosFlag ? 0 : 1));
		appendUChar(blr_parameter);
		appendUChar(1);
		appendUShort(USHORT(2 * outputVariables.getCount()));
	}

	appendUChar(blr_end);
}

void DsqlCompilerScratch::genParameters(Array<NestConst<ParameterClause> >& parameters,
	Array<NestConst<ParameterClause> >& returns)
{
	if (parameters.hasData())
	{
		fb_assert(parameters.getCount() < MAX_USHORT / 2);
		appendUChar(blr_message);
		appendUChar(0);
		appendUShort(2 * parameters.getCount());

		for (FB_SIZE_T i = 0; i < parameters.getCount(); ++i)
		{
			ParameterClause* parameter = parameters[i];
			putDebugArgument(fb_dbg_arg_input, i, parameter->name.c_str());
			putType(parameter->type, true);

			// Add slot for null flag (parameter2).
			appendUChar(blr_short);
			appendUChar(0);

			makeVariable(parameter->type, parameter->name.c_str(),
				dsql_var::TYPE_INPUT, 0, (USHORT) (2 * i), 0);
		}
	}

	fb_assert(returns.getCount() < MAX_USHORT / 2);
	appendUChar(blr_message);
	appendUChar(1);
	appendUShort(2 * returns.getCount() + 1);

	if (returns.hasData())
	{
		for (FB_SIZE_T i = 0; i < returns.getCount(); ++i)
		{
			ParameterClause* parameter = returns[i];
			putDebugArgument(fb_dbg_arg_output, i, parameter->name.c_str());
			putType(parameter->type, true);

			// Add slot for null flag (parameter2).
			appendUChar(blr_short);
			appendUChar(0);

			makeVariable(parameter->type, parameter->name.c_str(),
				dsql_var::TYPE_OUTPUT, 1, (USHORT) (2 * i), i);
		}
	}

	// Add slot for EOS.
	appendUChar(blr_short);
	appendUChar(0);
}

void DsqlCompilerScratch::addCTEs(WithClause* withClause)
{
	if (ctes.getCount())
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // WITH clause can't be nested
				  Arg::Gds(isc_dsql_cte_nested_with));
	}

	if (withClause->recursive)
		flags |= DsqlCompilerScratch::FLAG_RECURSIVE_CTE;

	const SelectExprNode* const* end = withClause->end();

	for (SelectExprNode* const* cte = withClause->begin(); cte != end; ++cte)
	{
		if (withClause->recursive)
		{
			currCtes.push(*cte);
			PsqlChanger changer(this, false);
			ctes.add(pass1RecursiveCte(*cte));
			currCtes.pop();

			// Add CTE name into CTE aliases stack. It allows later to search for
			// aliases of given CTE.
			addCTEAlias((*cte)->alias);
		}
		else
			ctes.add(*cte);
	}
}

SelectExprNode* DsqlCompilerScratch::findCTE(const MetaName& name)
{
	for (FB_SIZE_T i = 0; i < ctes.getCount(); ++i)
	{
		SelectExprNode* cte = ctes[i];

		if (cte->alias == name.c_str())
			return cte;
	}

	return NULL;
}

void DsqlCompilerScratch::clearCTEs()
{
	flags &= ~DsqlCompilerScratch::FLAG_RECURSIVE_CTE;
	ctes.clear();
	cteAliases.clear();
	currCteAlias = NULL;
}

// Look for unused CTEs and issue a warning about its presence. Also, make DSQL
// pass of every found unused CTE to check all references and initialize input
// parameters. Note, when passing some unused CTE which refers to another unused
// (by the main query) CTE, "unused" flag of the second one is cleared. Therefore
// names is collected in separate step.
void DsqlCompilerScratch::checkUnusedCTEs()
{
	bool sqlWarn = false;
	FB_SIZE_T i;

	for (i = 0; i < ctes.getCount(); ++i)
	{
		SelectExprNode* cte = ctes[i];

		if (!(cte->dsqlFlags & RecordSourceNode::DFLAG_DT_CTE_USED))
		{
			if (!sqlWarn)
			{
				ERRD_post_warning(Arg::Warning(isc_sqlwarn) << Arg::Num(-104));
				sqlWarn = true;
			}

			ERRD_post_warning(Arg::Warning(isc_dsql_cte_not_used) << cte->alias);
		}
	}

	for (i = 0; i < ctes.getCount(); ++i)
	{
		SelectExprNode* cte = ctes[i];

		if (!(cte->dsqlFlags & RecordSourceNode::DFLAG_DT_CTE_USED))
			cte->dsqlPass(this);
	}
}

DeclareSubFuncNode* DsqlCompilerScratch::getSubFunction(const MetaName& name)
{
	DeclareSubFuncNode* subFunc = NULL;
	subFunctions.get(name, subFunc);

	if (!subFunc && mainScratch)
		subFunc = mainScratch->getSubFunction(name);

	return subFunc;
}

void DsqlCompilerScratch::putSubFunction(DeclareSubFuncNode* subFunc, bool replace)
{
	if (!replace && subFunctions.exist(subFunc->name))
	{
		status_exception::raise(
			Arg::Gds(isc_dsql_duplicate_spec) << subFunc->name);
	}

	subFunctions.put(subFunc->name, subFunc);
}

DeclareSubProcNode* DsqlCompilerScratch::getSubProcedure(const MetaName& name)
{
	DeclareSubProcNode* subProc = NULL;
	subProcedures.get(name, subProc);

	if (!subProc && mainScratch)
		subProc = mainScratch->getSubProcedure(name);

	return subProc;
}

void DsqlCompilerScratch::putSubProcedure(DeclareSubProcNode* subProc, bool replace)
{
	if (!replace && subProcedures.exist(subProc->name))
	{
		status_exception::raise(
			Arg::Gds(isc_dsql_duplicate_spec) << subProc->name);
	}

	subProcedures.put(subProc->name, subProc);
}

// Process derived table which can be recursive CTE.
// If it is non-recursive return input node unchanged.
// If it is recursive return new derived table which is an union of union of anchor (non-recursive)
// queries and union of recursive queries. Check recursive queries to satisfy various criteria.
// Note that our parser is right-to-left therefore nested list linked as first node in parent list
// and second node is always query spec.
// For example, if we have 4 CTE's where first two are non-recursive and last two are recursive:
//
//				list							  union
//			  [0]	[1]						   [0]		[1]
//			list	cte4		===>		anchor		recursive
//		  [0]	[1]						 [0]	[1]		[0]		[1]
//		list	cte3					cte1	cte2	cte3	cte4
//	  [0]	[1]
//	cte1	cte2
//
// Also, we should not change layout of original parse tree to allow it to be parsed again if
// needed. Therefore recursive part is built using newly allocated list nodes.
SelectExprNode* DsqlCompilerScratch::pass1RecursiveCte(SelectExprNode* input)
{
	RecordSourceNode* const query = input->querySpec;
	UnionSourceNode* unionQuery = nodeAs<UnionSourceNode>(query);

	if (!unionQuery)
	{
		if (!pass1RseIsRecursive(nodeAs<RseNode>(query)))
			return input;

		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive CTE (%s) must be an UNION
				  Arg::Gds(isc_dsql_cte_not_a_union) << input->alias);
	}

	// Split queries list on two parts: anchor and recursive.
	// In turn, the anchor part consists of:
	//   - a number of rightmost nodes combined by UNION ALL
	//   - the leftmost node representing a union of all other nodes

	MemoryPool& pool = getPool();

	unsigned anchors = 0;
	RecordSourceNode* anchorRse = NULL;
	Stack<RseNode*> anchorStack(pool);
	Stack<RseNode*> recursiveStack(pool);

	NestConst<RecordSourceNode>* iter = unionQuery->dsqlClauses->items.end();
	bool dsqlAll = unionQuery->dsqlAll;

	while (unionQuery)
	{
		RecordSourceNode* clause = *--iter;

		if (iter == unionQuery->dsqlClauses->items.begin())
		{
			unionQuery = nodeAs<UnionSourceNode>(clause);

			if (unionQuery)
			{
				iter = unionQuery->dsqlClauses->items.end();
				dsqlAll = unionQuery->dsqlAll;

				clause = *--iter;

				if (!anchorRse && !dsqlAll)
					anchorRse = unionQuery;
			}
		}

		RseNode* const rse = nodeAs<RseNode>(clause);
		fb_assert(rse);
		RseNode* const newRse = pass1RseIsRecursive(rse);

		if (newRse) // rse is recursive
		{
			if (anchors)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// CTE '%s' defined non-recursive member after recursive
					Arg::Gds(isc_dsql_cte_nonrecurs_after_recurs) << input->alias);
			}

			if (newRse->dsqlDistinct)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive member of CTE '%s' has %s clause
					Arg::Gds(isc_dsql_cte_wrong_clause) << input->alias <<
														   Arg::Str("DISTINCT"));
			}

			if (newRse->dsqlGroup)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive member of CTE '%s' has %s clause
					Arg::Gds(isc_dsql_cte_wrong_clause) << input->alias <<
														   Arg::Str("GROUP BY"));
			}

			if (newRse->dsqlHaving)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive member of CTE '%s' has %s clause
					Arg::Gds(isc_dsql_cte_wrong_clause) << input->alias <<
														   Arg::Str("HAVING"));
			}

			if (!dsqlAll)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive members of CTE (%s) must be linked with another members via UNION ALL
					Arg::Gds(isc_dsql_cte_union_all) << input->alias);
			}

			recursiveStack.push(newRse);
		}
		else
		{
			anchors++;

			if (!anchorRse && dsqlAll)
				anchorStack.push(rse);
		}
	}

	// If there's no recursive part, then return the input node as is

	if (!recursiveStack.hasData())
		return input;

	// Merge anchor parts into a single node

	if (anchorStack.hasData())
	{
		if (anchorStack.getCount() > 1)
		{
			RecordSourceNode* const firstNode = anchorRse ? anchorRse : anchorStack.pop();
			UnionSourceNode* const rse = FB_NEW_POOL(pool) UnionSourceNode(pool);
			rse->dsqlClauses = FB_NEW_POOL(pool) RecSourceListNode(pool, firstNode);
			rse->dsqlAll = true;
			rse->recursive = false;

			while (anchorStack.hasData())
				rse->dsqlClauses->add(anchorStack.pop());

			anchorRse = rse;
		}
		else
			anchorRse = anchorStack.pop();
	}
	else if (!anchorRse)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			// Non-recursive member is missing in CTE '%s'
			Arg::Gds(isc_dsql_cte_miss_nonrecursive) << input->alias);
	}

	// Merge recursive parts into a single node

	RecordSourceNode* recursiveRse = NULL;

	if (recursiveStack.getCount() > 1)
	{
		UnionSourceNode* const rse = FB_NEW_POOL(pool) UnionSourceNode(pool);
		rse->dsqlClauses = FB_NEW_POOL(pool) RecSourceListNode(pool, (unsigned) 0);
		rse->dsqlAll = true;
		rse->recursive = false;

		while (recursiveStack.hasData())
		{
			RseNode* const sub_rse = recursiveStack.pop();
			sub_rse->dsqlFlags |= RecordSourceNode::DFLAG_RECURSIVE;
			rse->dsqlClauses->add(sub_rse);
		}

		recursiveRse = rse;
	}
	else
	{
		recursiveRse = recursiveStack.pop();
		recursiveRse->dsqlFlags |= RecordSourceNode::DFLAG_RECURSIVE;
	}

	// Create and return the final node

	UnionSourceNode* unionNode = FB_NEW_POOL(pool) UnionSourceNode(pool);
	unionNode->dsqlAll = true;
	unionNode->recursive = true;
	unionNode->dsqlClauses = FB_NEW_POOL(pool) RecSourceListNode(pool, 2);
	unionNode->dsqlClauses->items[0] = anchorRse;
	unionNode->dsqlClauses->items[1] = recursiveRse;

	SelectExprNode* select = FB_NEW_POOL(getPool()) SelectExprNode(getPool());
	select->querySpec = unionNode;

	select->alias = input->alias;
	select->columns = input->columns;

	return select;
}

// Check if rse is recursive. If recursive reference is a table in the FROM list remove it.
// If recursive reference is a part of join add join boolean (returned by pass1JoinIsRecursive)
// to the WHERE clause. Punt if more than one recursive reference is found.
RseNode* DsqlCompilerScratch::pass1RseIsRecursive(RseNode* input)
{
	MemoryPool& pool = getPool();

	RseNode* result = FB_NEW_POOL(getPool()) RseNode(getPool());
	result->dsqlFirst = input->dsqlFirst;
	result->dsqlSkip = input->dsqlSkip;
	result->dsqlDistinct = input->dsqlDistinct;
	result->dsqlSelectList = input->dsqlSelectList;
	result->dsqlWhere = input->dsqlWhere;
	result->dsqlGroup = input->dsqlGroup;
	result->dsqlHaving = input->dsqlHaving;
	result->rse_plan = input->rse_plan;

	RecSourceListNode* srcTables = input->dsqlFrom;
	RecSourceListNode* dstTables = FB_NEW_POOL(pool) RecSourceListNode(pool, srcTables->items.getCount());
	result->dsqlFrom = dstTables;

	NestConst<RecordSourceNode>* pDstTable = dstTables->items.begin();
	NestConst<RecordSourceNode>* pSrcTable = srcTables->items.begin();
	NestConst<RecordSourceNode>* end = srcTables->items.end();
	bool found = false;

	for (NestConst<RecordSourceNode>* prev = pDstTable; pSrcTable < end; ++pSrcTable, ++pDstTable)
	{
		*prev++ = *pDstTable = *pSrcTable;

		RseNode* rseNode = nodeAs<RseNode>(*pDstTable);

		if (rseNode)
		{
			fb_assert(rseNode->dsqlExplicitJoin);

			RseNode* dstRse = rseNode->clone(getPool());

			*pDstTable = dstRse;

			BoolExprNode* joinBool = pass1JoinIsRecursive(*pDstTable->getAddress());

			if (joinBool)
			{
				if (found)
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  // Recursive member of CTE can't reference itself more than once
							  Arg::Gds(isc_dsql_cte_mult_references));
				}

				found = true;

				result->dsqlWhere = PASS1_compose(result->dsqlWhere, joinBool, blr_and);
			}
		}
		else if (nodeIs<ProcedureSourceNode>(*pDstTable) || nodeIs<RelationSourceNode>(*pDstTable))
		//// TODO: LocalTableSourceNode
		{
			if (pass1RelProcIsRecursive(*pDstTable))
			{
				if (found)
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  // Recursive member of CTE can't reference itself more than once
							  Arg::Gds(isc_dsql_cte_mult_references));
				}
				found = true;

				--prev;
				dstTables->items.pop();
			}
		}
		else
			fb_assert(nodeIs<SelectExprNode>(*pDstTable));
	}

	if (found)
		return result;

	return NULL;
}

// Check if table reference is recursive i.e. its name is equal to the name of current processing CTE.
bool DsqlCompilerScratch::pass1RelProcIsRecursive(RecordSourceNode* input)
{
	QualifiedName relName;
	string relAlias;

	if (auto procNode = nodeAs<ProcedureSourceNode>(input))
	{
		relName = procNode->dsqlName;
		relAlias = procNode->alias;
	}
	else if (auto relNode = nodeAs<RelationSourceNode>(input))
	{
		relName = relNode->dsqlName;
		relAlias = relNode->alias;
	}
	else if (auto tableValueFunctionNode = nodeAs<TableValueFunctionSourceNode>(input))
	{
		relName.object = tableValueFunctionNode->dsqlName;
		relAlias = tableValueFunctionNode->alias.c_str();
	}
	//// TODO: LocalTableSourceNode
	else
		return false;

	fb_assert(currCtes.hasData());
	const SelectExprNode* currCte = currCtes.object();
	const bool recursive = relName.schema.isEmpty() && currCte->alias == relName.object.c_str();

	if (recursive)
		addCTEAlias(relAlias.hasData() ? relAlias.c_str() : relName.object.c_str());

	return recursive;
}

// Check if join have recursive members. If found remove this member from join and return its
// boolean (to be added into WHERE clause).
// We must remove member only if it is a table reference. Punt if recursive reference is found in
// outer join or more than one recursive reference is found
BoolExprNode* DsqlCompilerScratch::pass1JoinIsRecursive(RecordSourceNode*& input)
{
	RseNode* inputRse = nodeAs<RseNode>(input);
	fb_assert(inputRse);

	const UCHAR joinType = inputRse->rse_jointype;
	bool remove = false;

	bool leftRecursive = false;
	BoolExprNode* leftBool = NULL;
	NestConst<RecordSourceNode>* joinTable = &inputRse->dsqlFrom->items[0];
	RseNode* joinRse;

	if ((joinRse = nodeAs<RseNode>(*joinTable)) && joinRse->dsqlExplicitJoin)
	{
		leftBool = pass1JoinIsRecursive(*joinTable->getAddress());
		leftRecursive = (leftBool != NULL);
	}
	else
	{
		leftBool = inputRse->dsqlWhere;
		leftRecursive = pass1RelProcIsRecursive(*joinTable);

		if (leftRecursive)
			remove = true;
	}

	if (leftRecursive && joinType != blr_inner)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive member of CTE can't be member of an outer join
				  Arg::Gds(isc_dsql_cte_outer_join));
	}

	bool rightRecursive = false;
	BoolExprNode* rightBool = NULL;

	joinTable = &inputRse->dsqlFrom->items[1];

	if ((joinRse = nodeAs<RseNode>(*joinTable)) && joinRse->dsqlExplicitJoin)
	{
		rightBool = pass1JoinIsRecursive(*joinTable->getAddress());
		rightRecursive = (rightBool != NULL);
	}
	else
	{
		rightBool = inputRse->dsqlWhere;
		rightRecursive = pass1RelProcIsRecursive(*joinTable->getAddress());

		if (rightRecursive)
			remove = true;
	}

	if (rightRecursive && joinType != blr_inner)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive member of CTE can't be member of an outer join
				  Arg::Gds(isc_dsql_cte_outer_join));
	}

	if (leftRecursive && rightRecursive)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive member of CTE can't reference itself more than once
				  Arg::Gds(isc_dsql_cte_mult_references));
	}

	if (leftRecursive)
	{
		if (remove)
			input = inputRse->dsqlFrom->items[1];

		return leftBool;
	}

	if (rightRecursive)
	{
		if (remove)
			input = inputRse->dsqlFrom->items[0];

		return rightBool;
	}

	return NULL;
}

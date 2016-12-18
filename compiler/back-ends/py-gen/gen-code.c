/*
 * compiler/back-ends/py-gen/gen-code.c - routines for printing python
 *                                        code from type trees
 *
 * assumes that the type tree has already been run through the
 * python type generator (py-gen/types.c).
 *
 * Copyright (C) 2016 Aaron Conole
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include "snacc.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <string.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "rules.h"
#include "snacc-util.h"
#include "print.h"
#include "tag-util.h"  /* get GetTags/FreeTags/CountTags/TagByteLen */

#if META
#include "meta.h"
#endif


enum BasicTypeChoiceId ParanoidGetBuiltinType PARAMS ((t),Type *t);
void PrintPyAnyCode PROTO ((FILE *src, FILE *hdr, PyRules *r,
							ModuleList *mods, Module *m));
void PrintPyValueDef PROTO ((FILE *src, PyRules *r, ValueDef *v));
void PrintPyValueExtern PROTO ((FILE *hdr, PyRules *r, ValueDef *v));
void PrintPySetTypeByCode(NamedType *defByNamedType, CxxTRI *cxxtri, FILE *src);
void PrintPyEncodeContaining(Type *t, PyRules *r, FILE *src);
void PrintPyPEREncodeContaining(Type *t, PyRules *r, FILE *src);
void PrintPyPERDecodeContaining(Type *t, PyRules *r, FILE *src);
void PrintPyDecodeContaining(Type *t, PyRules *r, FILE *src);
static char *
LookupNamespace PROTO ((Type *t, ModuleList *mods));
static void PrintPySeqSetPrintFunction(FILE* src, FILE* hdr,
                                       MyString className,
                                       BasicType *pBasicType);
static void PrintPyDefCode_PERSort(NamedType ***pppElementNamedType,
                                   int **ppElementTag,
                                   AsnList *pElementList);
static void PrintPyDefCode_SetSeqPEREncode(FILE *src, FILE *hdr, PyRules *r,
                                           TypeDef *td,
                                           NamedType **pSetElementNamedType,
                                           int iElementCount);
static void PrintPyDefCode_SetSeqPERDecode(FILE *src, FILE *hdr,
                                           PyRules *r, TypeDef *td,
                                           NamedType **pSetElementNamedType,
                                           int iElementCount);


/* flag to see if constraints were present */
static int constraints_flag;
static long lconstraintvar=0;

extern char *bVDAGlobalDLLExport;
extern int gNO_NAMESPACE;
extern const char *gAlternateNamespaceString;
extern int genPERCode;
//extern short ImportedFilesG;

static const char bufTypeNameG[] = "asn_buffer.AsnBuf";
static const char lenTypeNameG[] = "AsnLen";
static const char tagTypeNameG[] = "AsnTag";
static const char baseClassesG[] = "(asn_base.AsnBase)";

static int printTypesG;
static int printEncodersG;
static int printDecodersG;
static int printPrintersG;
static int printFreeG;

// normalizeValue
// 
// strip whitespace and { } from valueNation values.
//
static void
normalizeValue(char **normalized, char *input)
{
   int i;
   while(*input == ' ' || *input == '{' )
      input++;

   *normalized = strdup(input);
   
   i = strlen(*normalized) - 1;
   while ( (*normalized)[i] == ' ' || (*normalized)[i] == '}' )
   {
      (*normalized)[i] = 0;
      i--;
   }
}

static char *GetImportFileName (char *Impname, ModuleList *mods)
{
    Module *currMod;
    char *fileName = NULL;
    FOR_EACH_LIST_ELMT (currMod, mods) {
        /* Find the import Module in the Modules and
         * return the header file name
         */
        if ((strcmp(Impname, currMod->modId->name) == 0)) {
            /* Set the file name and break */
            fileName = currMod->cxxHdrFileName;
            break;
        }
    }
    return fileName;
}

static Module *GetImportModuleRef (char *Impname, ModuleList *mods)
{
    Module *currMod=NULL;
    FOR_EACH_LIST_ELMT (currMod, mods) {
        /* Find the import Module in the Modules and
         * return the header file name
         */
        if ((strcmp(Impname, currMod->modId->name) == 0)) {
            break;
        }
    }
    return currMod;
}

static void
PrintSimpleMeta(FILE *hdr, char *className,int exportMember)
{

}

static void
PrintSimpleCheck(FILE *hdr, FILE* src, char *className, int exportMember)
{
	fprintf(src, "int %s::checkConstraints(ConstraintFailList* pConstraintFails) const\n{\n",
		className);
	fprintf(src, "\treturn checkListConstraints(pConstraintFails);\n");
	fprintf(src, "}\n");
}

static void
PrintSrcComment PARAMS ((src, m),
    FILE *src _AND_
    Module *m)
{
    time_t now = time (NULL);

    fprintf (src, "# %s - class definitions for ASN.1 module %s\n",
             m->cxxSrcFileName, m->modId->name);
    fprintf (src, "#\n");
    fprintf (src, "#   This file was generated by esnacc on %s\n",
             ctime(&now));
    fprintf (src, "#   NOTE: this is a machine generated file-"
             "-editing not recommended\n");
    fprintf (src, "\n");

} /* PrintSrcComment */


static void
PrintSrcIncludes PARAMS ((src), FILE *src)
{
    fprintf(src, "from esnacc import asn_base\n");
    fprintf(src, "from esnacc import asn_bool\n");
    fprintf(src, "from esnacc import asn_buffer\n");
    fprintf(src, "from esnacc import asn_ints\n");
    fprintf(src, "from esnacc import asn_list\n");
    fprintf(src, "from esnacc import asn_octs\n");
    fprintf(src, "from esnacc import asn_useful\n");
    fprintf(src, "\n");
} /* PrintSrcIncludes */


static void
PrintTypeDecl PARAMS ((f, td),
    FILE *f _AND_
    TypeDef *td)
{
    switch (td->type->basicType->choiceId)
    {   
        case BASICTYPE_COMPONENTSOF:
        case BASICTYPE_SELECTION:
        case BASICTYPE_UNKNOWN:
        case BASICTYPE_MACRODEF:
        case BASICTYPE_MACROTYPE:
            return; /* do nothing */

        default:
            if (IsNewType (td->type))
                fprintf(f, "class %s;\n", td->cxxTypeDefInfo->className);
    }
	
} /* PrintTypeDecl */


static void
PrintPyType PARAMS ((hdr, mods, m, r, td, parent, t),
    FILE *hdr _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    PyRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t)
{
    char *pszNamespace=NULL;
    pszNamespace = LookupNamespace(t, mods);

    if (pszNamespace) {
        fprintf(hdr, "%s::%s       ",
                pszNamespace, t->cxxTypeRefInfo->className);
    } else {
        fprintf(hdr, "%s       ", t->cxxTypeRefInfo->className);
    }

    if (t->cxxTypeRefInfo->isPtr)
        fprintf (hdr, "*");

} /* PrintPyType */



/*
 *  Uses the Constructor that takes no args.
 *  Assumes file f is positioned inside a class definition.
 *  All Classes get this to support the ANY type.
 */
static void
PrintCloneMethod PARAMS ((hdr, src, td),
    FILE *hdr _AND_
    FILE *src _AND_
    TypeDef *td)
{
//    fprintf (hdr, "  AsnType		*Clone() const;\n\n", td->cxxTypeDefInfo->className);
//    fprintf (hdr, "  AsnType		*Clone() const;\n\n");
    fprintf (src, "AsnType *%s::Clone() const\n", td->cxxTypeDefInfo->className);
    fprintf (src, "{\n");
    fprintf (src, "  return new %s(*this);\n", td->cxxTypeDefInfo->className);
    fprintf (src, "}\n\n");

} /* PrintCloneMethod */


/*
 * prints inline definition of constructors if this class is
 * derived from a library class.
 * assumes FILE *f is positioned in the derived class definition (.h)
 *
 * 12/92 MS - added overloaded "=" ops for string types.
 */
static void
PrintDerivedConstructors PARAMS ((f, td),
    FILE *f _AND_
    TypeDef *td)
{
    char *baseClassName = td->type->cxxTypeRefInfo->className;
    fprintf(f,
            "    def __init__(self, value=None):\n"
            "        %s.__init__(self,value)\n\n", baseClassName);
} /* PrintDerivedConstructors */

#if DEPRECATED
static void
PrintPyEocEncoders PARAMS ((src, td, t, bufVarName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *t _AND_
    char *bufVarName)
{
    TagList *tl;
    Tag *tag;
    int stoleChoiceTags;
    
    /* get all the tags on this type*/
    tl = (TagList*) GetTags (t, &stoleChoiceTags);
    FreeTags (tl);
    bufVarName=bufVarName;td=td;  /*AVOIDS warning.*/

} /* PrintPyEocEncoders */
#endif

static int
HasShortLen PARAMS ((t),
    Type *t)
{
    enum BasicTypeChoiceId typesType;
    /*
     * efficiency hack - use simple length (1 byte)
     * encoded for type (almost) guaranteed to have
     * encoded lengths of 0 <= len <= 127
     */
    typesType = GetBuiltinType (t);
    /*RWC;8/9/01;REMOVED when INTEGER made AsnOcts;return typesType == BASICTYPE_BOOLEAN || typesType == BASICTYPE_INTEGER || typesType == BASICTYPE_NULL || typesType == BASICTYPE_REAL || typesType == BASICTYPE_ENUMERATED; */
    return typesType == BASICTYPE_BOOLEAN || typesType == BASICTYPE_NULL || typesType == BASICTYPE_REAL || typesType == BASICTYPE_ENUMERATED;
} /* HasShortLen */


/*
 * prints length encoding code.  Primitives always use
 * definite length and constructors get "ConsLen"
 * which can be configured at compile to to be indefinite
 * or definite.  Primitives can also be "short" (isShort is true)
 * in which case a fast macro is used to write the length.
 * Types for which isShort apply are: boolean, null and
 * (almost always) integer and reals
 */
static void
PrintPyLenEncodingCode PARAMS ((f, lenVarName, bufVarName, classStr,
                                formStr),
                               FILE *f _AND_
                               char *lenVarName _AND_
                               char *bufVarName _AND_
                               char *classStr _AND_
                               char *formStr)
{
    fprintf(f, "        %s += asn_buffer.BEncDefLen(%s, %s)\n",
            lenVarName, bufVarName, lenVarName);
    fprintf(f, "        TAG_CODE = asn_base.BERConsts.MakeTag(asn_base.BERConsts.%s,\n"
             "                                              self.%s,\n"
             "                                              self.BER_TAG)\n",
             classStr, formStr);
}

/* prints last tag's encoding code first */
static void
PrintPyTagAndLenList PARAMS ((src, tagList, lenVarName, bufVarName),
    FILE *src _AND_
    TagList *tagList _AND_
    char *lenVarName _AND_
    char *bufVarName)
{
    char *classStr;
    char *formStr;
    Tag *tg;
    int tagLen;

    if ((tagList == NULL) || LIST_EMPTY (tagList))
        return;

    /*
     * since encoding backward encode tags backwards
     */
    FOR_EACH_LIST_ELMT_RVS (tg, tagList) {
        classStr = Class2ClassStr (tg->tclass);

        if (tg->form == CONS) {
            formStr = Form2FormStr (CONS);
        } else {
            formStr = Form2FormStr (PRIM);
        }
        PrintPyLenEncodingCode (src, lenVarName, bufVarName,
                                classStr, formStr);
        fprintf (src, "\n");

        if (tg->tclass == UNIV) {
            const char* ptr = DetermineCode(tg, &tagLen, 0);
            fprintf (src, "    %s += BEncTag%d (%s, %s, %s, %s);\n", lenVarName, tagLen, bufVarName, classStr, formStr, ptr);
        } else {
            const char* ptr = DetermineCode(tg, &tagLen, 1);
            fprintf(src, "    %s += BEncTag%d(%s, %s, %s, %s);\n", lenVarName, tagLen, bufVarName, classStr, formStr, ptr);
        }                                                       //RWC;tg->code);
    }
} /* PrintPyTagAndLenList */

/*
 *  Recursively walks through tags, printing lower lvl tags
 *  first (since encoding is done backwards).
 *
 */
static void
PrintPyTagAndLenEncodingCode PARAMS ((src, t, lenVarName, bufVarName),
    FILE *src _AND_
    Type *t _AND_
    char *lenVarName _AND_
    char *bufVarName)
{
    TagList *tl;
    int stoleChoiceTags;

    /*
     * get all the tags on this type
     */
    tl = (TagList*) GetTags(t, &stoleChoiceTags);

    /*
     * leave choice elmt tag enc to encoding routine
     */
    if (!stoleChoiceTags)
        PrintPyTagAndLenList(src, tl, lenVarName, bufVarName);

    FreeTags(tl);
} /* PrintPyTagAndLenEncodingCode */


/*
 * used to figure out local variables to declare
 * for decoding tags/len pairs on type t
 */
static int
CxxCountVariableLevels PARAMS ((t),
    Type *t)
{
    if (GetBuiltinType (t) == BASICTYPE_CHOICE)
        return CountTags (t) +1; /* since must decode 1 internal tag type */
    else
        return CountTags (t);
} /* CxxCountVariableLevels */


/*
 * returns true if elmts curr following
 *  onward are all optional ow. false
 */
static int
RestAreTailOptional PARAMS ((e),
    NamedTypeList *e)
{
    NamedType *elmt;
    void *tmp;
    int retVal;

    if (e == NULL)
        return TRUE;

    tmp = (void*)CURR_LIST_NODE (e);
    retVal = TRUE;
    AsnListNext (e);
    FOR_REST_LIST_ELMT (elmt, e)
    {
        if ((!elmt->type->optional) && (elmt->type->defaultVal == NULL)&&(!elmt->type->extensionAddition))
        {
            retVal = FALSE;
            break;
        }
    }
    SET_CURR_LIST_NODE (e, tmp); /* reset list to orig loc */
    return retVal;
}


/*
 * prints typedef or new class given an ASN.1  type def of a primitive type
 * or typeref.  Uses inheritance to cover re-tagging and named elmts.
 */
static void
PrintPySimpleDef PARAMS ((hdr, src, r, td),
                         FILE *hdr _AND_
                         FILE *src _AND_
                         PyRules *r _AND_
                         TypeDef *td)
{
    Tag *tag;
    TagList *tags;
    char *formStr;
    char *classStr;
    int i;
    CNamedElmt *n;
    int stoleChoiceTags;
    int elmtLevel;
    enum BasicTypeChoiceId typeId;

    if (IsNewType (td->type)) {
        int	hasNamedElmts;

        fprintf(src, "class %s(%s):\n",
                td->cxxTypeDefInfo->className,
                td->type->cxxTypeRefInfo->className);

        /*
         * must explicitly call constructors for base class
         */
        PrintDerivedConstructors (src, td);
        if ((hasNamedElmts = HasNamedElmts (td->type)) != 0) {   
            int count = 0;
            fprintf(src, "    ENUMERATIONS = Enum(");
            FOR_EACH_LIST_ELMT (n, td->type->cxxTypeRefInfo->namedElmts) {
                fprintf(src, "%s = %d", n->name, n->value);
                if (n != (CNamedElmt *)LAST_LIST_ELMT(
                        td->type->cxxTypeRefInfo->namedElmts))
                    fprintf (src, ",");
                else
                    fprintf (src, ")\n");
                count++;
            }
        }

        fprintf(src, "    def typename(self):\n");
        fprintf(src, "        return \"%s\"\n\n",
                td->cxxTypeDefInfo->className);
        /*
         * Re-do BerEncode, BerDeocode, BerDecodePdu and BerDecodePdu
         * if this type has been re-tagged
         */
        if ((IsDefinedByLibraryType(td->type) && !HasDefaultTag(td->type))
            || (IsTypeRef (td->type) && ((td->type->tags != NULL) 
                                         && !LIST_EMPTY(td->type->tags)))) {

            int tagLen = 0;
            tags = GetTags (td->type, &stoleChoiceTags);
            if (tags->count > 1) {
                fprintf(src, "    # WARNING: only one tag added...\n");
            }
            tag = tags->first->data;
            typeId = GetBuiltinType (td->type);
            classStr = Class2ClassStr (tag->tclass);
            formStr = Form2FormStr((tag->form == ANY_FORM) ? PRIM : tag->form);
            fprintf(src, "    BER_FORM = asn_base.BERConsts.%s\n", formStr);
            fprintf(src, "    BER_CLASS = asn_base.BERConsts.%s\n", classStr);
            fflush(src);
            if (!stoleChoiceTags) {
                const char *ptr = DetermineCode(tag, &tagLen, (tag->tclass == UNIV) ? 0 : 1);
                fprintf(src, "    BER_TAG = %s\n", ptr);
            }
            FreeTags (tags);
            fprintf(src, "\n\n");
        }
    } else {


		/* JKG 7/31/03 */
		/* The following code enclosed in this if/else statement */
		/* is constructed for constraint handling capability     */
		/* for primitives found outside of                       */
		/* sequences or sets                                     */

        if (td->type->subtypes != NULL) {
            switch (td->type->subtypes->choiceId) {
            case SUBTYPE_AND:
            case SUBTYPE_OR:
            case SUBTYPE_SINGLE:
                {   
                    struct NamedType temp;
                    NamedType* tmp = &temp;
                    tmp->type=td->type;
                    tmp->type->cxxTypeRefInfo->fieldName=td->definedName;
                    tmp->fieldName=td->definedName;
#if 0
                    if (!PrintPyMultiConstraintOrHandler(hdr, src, NULL, tmp, 0))
                        PrintTypeDefDefault(hdr, src, td);
#endif
                }
                break;
            default:
#if 0
                PrintTypeDefDefault(hdr, src, td);
#endif
                break;
			}
		} else {
#if 0
            PrintTypeDefDefault(hdr, src, td);
#endif
        }
    }

} /* PrintPySimpleDef */

static void
PrintPyChoiceDefCode (FILE *src, FILE *hdr, ModuleList *mods, Module *m, PyRules *r ,
    TypeDef *td,Type *parent, Type *choice, int novolatilefuncs)
{
    NamedType *e;
    char *classStr;
    char *formStr;
    char *codeStr;
    int tagLen=0, i;
    Tag *tag;
    TagList *tags;
    char *varName;
    CxxTRI *cxxtri;
    int elmtLevel=0;
    int varCount, tmpVarCount;
    int stoleChoiceTags;
    enum BasicTypeChoiceId tmpTypeId;
    NamedType *defByNamedType;
    NamedType **ppElementNamedType;
    int *pElementTag;
    int ii;
    char *ptr="";   /* NOT DLL Exported, or ignored on Unix. */
    int extensionsExist = FALSE;

    /* put class spec in hdr file */
    
    if (bVDAGlobalDLLExport != NULL) 
       ptr = bVDAGlobalDLLExport;

    fprintf(src, "class %s %s%s:\n", ptr, td->cxxTypeDefInfo->className, baseClassesG);
    /* write out choiceId enum type */

    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice) {
        if(e->type->subtypes != NULL) {
            switch(e->type->subtypes->choiceId) {   
            case SUBTYPE_AND:
            case SUBTYPE_OR:
            case SUBTYPE_SINGLE:
#if 0
                PrintPyMultiConstraintOrHandler(hdr, src,
                                                td->cxxTypeDefInfo->className,
                                                e, 3);
#endif
            default:
                break;
            }
        }
    }

    fprintf(src, "    CHOICES_%s = \\\n", r->choiceIdEnumName);
    fprintf(src, "    {\\\n");
    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice) {
        fprintf(src, "    %s = %d",
                e->type->cxxTypeRefInfo->choiceIdSymbol,
                e->type->cxxTypeRefInfo->choiceIdValue);
        if (e != (NamedType*)LAST_LIST_ELMT (choice->basicType->a.choice))
            fprintf(src, ",\\\n");
        else
            fprintf(src, "\\\n");
    }
    fprintf (src, "    }\n");

    /* write out the choice element anonymous union */
    fprintf (src, "  union\n");
    fprintf (src, "  {\n");
    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice) {
        fprintf (src, "  ");
        PrintPyType(hdr, mods, m, r, td, choice, e->type);
	    fprintf (src, "%s;\n\n", e->type->cxxTypeRefInfo->fieldName);
    }
    fprintf (src, "  };\n\n");


    fprintf (src, "\n");

    /* constructors and destructor */
    fprintf (src, "   %s() {Init();}\n", td->cxxTypeDefInfo->className);

    /* PIERCE 8-22-2001 */
	PrintSimpleMeta(src,td->cxxTypeDefInfo->className,0);

	/* Init() member function*/
    fprintf (src, "void %s::Init(void)\n", td->cxxTypeDefInfo->className);
    fprintf (src, "{\n");
    fprintf (src, "   // initialize choice to no choiceId to first choice and set pointer to NULL\n");
    e = FIRST_LIST_ELMT (choice->basicType->a.choice);
    fprintf (src, "  choiceId = %sCid;\n", e->type->cxxTypeRefInfo->fieldName);
    fprintf (src, "  %s = NULL;\n", e->type->cxxTypeRefInfo->fieldName);
    fprintf (src, "}\n\n");
	fprintf(src, "\nint %s::checkConstraints(ConstraintFailList* pConstraintFails) const\n{\n",
		td->cxxTypeDefInfo->className);

	FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
	{
		if (e->type->cxxTypeRefInfo->isPtr)
		{
			fprintf(src, "\tif (%s != NULL)\n",
				e->type->cxxTypeRefInfo->fieldName);
			fprintf(src, "\t\t%s->checkConstraints(pConstraintFails);\n\n",
				e->type->cxxTypeRefInfo->fieldName);
		}
		else
		{
			fprintf(src, "\t%s.checkConstraints(pConstraintFails);\n\n",
				e->type->cxxTypeRefInfo->fieldName);
		}
	}
	fprintf(src, "\treturn 0;\n");
	fprintf(src, "}\n\n\n");

    /* virtual destructor*/
    fprintf (src, "void %s::Clear()\n", td->cxxTypeDefInfo->className);
    fprintf (src, "{\n");
    fprintf (src, "  switch (choiceId)\n");
    fprintf (src, "  {\n");

    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
    {
        tmpTypeId = GetBuiltinType (e->type);

	    fprintf (src, "    case %s:\n", e->type->cxxTypeRefInfo->choiceIdSymbol);
	    if (e->type->cxxTypeRefInfo->isPtr)
       {
	      fprintf (src, "      delete %s;\n", e->type->cxxTypeRefInfo->fieldName);
          fprintf (src, "      %s = NULL;\n", e->type->cxxTypeRefInfo->fieldName);
       }
        else if(!e->type->cxxTypeRefInfo->isPtr && 
            ((tmpTypeId == BASICTYPE_CHOICE) ||
                (tmpTypeId == BASICTYPE_SET) ||
                (tmpTypeId == BASICTYPE_SEQUENCE)) )
        {
            fprintf (src, "  %s.Clear();\n", e->type->cxxTypeRefInfo->fieldName);
        }
	    fprintf (src, "      break;\n");
    }

    fprintf (src, "  } // end of switch\n");
    fprintf (src, "} // end of Clear()\n");
    fprintf (src, "\n");

    /* print clone routine for ANY mgmt */
    PrintCloneMethod (hdr, src, td);

    fprintf (src, "%s &%s::operator = (const %s &that)\n",
             td->cxxTypeDefInfo->className,
             td->cxxTypeDefInfo->className,
             td->cxxTypeDefInfo->className);
    fprintf (src, "{\n");
    fprintf (src, "    if (this != &that) {\n");
    fprintf (src, "        Clear();\n");
   
    e = FIRST_LIST_ELMT (choice->basicType->a.choice);
    fprintf (src, "        if (that.%s != NULL) {\n",
             e->type->cxxTypeRefInfo->fieldName);
        fprintf (src, "            switch (choiceId = that.choiceId) {\n");

    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice) {
        fprintf (src, "                case %s:\n",
                 e->type->cxxTypeRefInfo->choiceIdSymbol);
        if (e->type->cxxTypeRefInfo->isPtr) {
            fprintf (src, "                    %s = new %s(*that.%s);\n",
                     e->type->cxxTypeRefInfo->fieldName,
                     e->type->cxxTypeRefInfo->className,
                     e->type->cxxTypeRefInfo->fieldName);
        } else {
            fprintf (src, "                    %s = that.%s;\n",
                     e->type->cxxTypeRefInfo->fieldName,
                     e->type->cxxTypeRefInfo->fieldName);
        }
        fprintf (src, "                break;\n");
    }

    fprintf (src, "            }\n");
    fprintf (src, "        }\n");
    fprintf (src, "    }\n");
    fprintf (src, "\n");
    fprintf (src, "    return *this;\n");
    fprintf (src, "}\n\n");

    /* BerEncodeContent */
    if (printEncodersG)
    {
        //fprintf (hdr, "  %s		B%s (%s &_b) const;\n", lenTypeNameG, r->encodeContentBaseName, bufTypeNameG);

        fprintf (src, "%s\n", lenTypeNameG);
        fprintf (src, "%s::B%s (%s &_b) const\n", td->cxxTypeDefInfo->className, r->encodeContentBaseName, bufTypeNameG);
        fprintf (src, "{\n");
        fprintf (src, "    FUNC(\"%s::B%s (%s &_b)\");\n", td->cxxTypeDefInfo->className, r->encodeContentBaseName, bufTypeNameG);

        /* print local vars */
        fprintf (src, "  %s l=0;\n", lenTypeNameG);

        fprintf (src, "  switch (%s)\n", r->choiceIdFieldName);
        fprintf (src, "  {\n");
        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            cxxtri =  e->type->cxxTypeRefInfo;
            fprintf (src, "    case %s:\n", cxxtri->choiceIdSymbol);

            varName = cxxtri->fieldName;

            /* eSNACC 1.5 does not encode indefinite length
             *
             * PrintPyEocEncoders (src, td, e->type, "_b");
             */

            /* encode content */
            tmpTypeId = GetBuiltinType (e->type);
            if (tmpTypeId == BASICTYPE_ANYDEFINEDBY)
            {
               defByNamedType = e->type->basicType->a.anyDefinedBy->link;
               PrintPySetTypeByCode(defByNamedType, cxxtri, src);
            }
            else if (tmpTypeId == BASICTYPE_ANY)
            {
                fprintf (src, "    l = %s", varName);
                if (cxxtri->isPtr)
                    fprintf (src, "->");
                else
                    fprintf (src, ".");

                fprintf (src, "B%s (_b);\n", r->encodeBaseName);
            }
            else if ( (tmpTypeId == BASICTYPE_OCTETCONTAINING) ||
                      (tmpTypeId == BASICTYPE_BITCONTAINING))
            {
               PrintPyEncodeContaining(e->type, r, src);
            }
            else if ( tmpTypeId == BASICTYPE_EXTENSION )
            {
                fprintf (src, "      l = %s", varName);
                if (cxxtri->isPtr)
                    fprintf (src, "->");
                else
                    fprintf (src, ".");

                fprintf (src, "B%s (_b);\n", r->encodeBaseName);
            }
            else
            {
                fprintf (src, "      l = %s", varName);
                if (cxxtri->isPtr)
                    fprintf (src, "->");
                else
                    fprintf (src, ".");

                fprintf (src, "B%s (_b);\n", r->encodeContentBaseName);
            }

            /* encode tag (s) & len (s) */
            PrintPyTagAndLenEncodingCode(src, e->type, "l", "_b");

            fprintf (src, "      break;\n\n");
        }
        fprintf (src, "      default:\n");
		fprintf (src, "         throw EXCEPT(\"Choice is empty\", ENCODE_ERROR);\n");
        fprintf (src, "  } // end switch\n");

        fprintf (src, "  return l;\n");
        fprintf (src, "} // %s::B%s\n\n\n", td->cxxTypeDefInfo->className, r->encodeContentBaseName);
    }
    /* end of BerEncodeContent method */

    /* BerDecodeContent */
    if (printDecodersG)
    {
        //fprintf (hdr, "  void			B%s (const %s &_b, %s tag, %s elmtLen, %s &bytesDecoded /*, s env*/);\n", r->decodeContentBaseName, bufTypeNameG, tagTypeNameG, lenTypeNameG, lenTypeNameG);//, envTypeNameG);
        fprintf (src, "void %s::B%s (const %s &_b, %s tag, %s elmtLen0, %s &bytesDecoded /*, s env*/)\n", td->cxxTypeDefInfo->className, r->decodeContentBaseName, bufTypeNameG, tagTypeNameG, lenTypeNameG, lenTypeNameG);//, envTypeNameG);
        fprintf (src, "{\n");
        fprintf (src, "   FUNC(\"%s::B%s()\");\n",td->cxxTypeDefInfo->className, r->decodeContentBaseName); 
        fprintf (src, "   Clear();\n");
        //fprintf (src, "   Init();\n");
        /* print local vars */
        /* count max number of extra length var nec
         * by counting tag/len pairs on components of the CHOICE
         */

        varCount = 0;
        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            tmpVarCount = CxxCountVariableLevels (e->type);
            if (tmpVarCount > varCount)
                varCount = tmpVarCount;
        }
        /* write extra length vars - remember choice content
         * decoders are passed the 'key' tag so need one less
         * than max var count.
         */
        for (i = 1; i < varCount; i++)
            fprintf (src, "  %s elmtLen%d = 0;\n", lenTypeNameG, i);

        /* switch on given tag - choices always have the key tag decoded */
        fprintf (src, "  switch (tag)\n");
        fprintf (src, "  {\n");
        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            if( e->type->basicType->choiceId == BASICTYPE_EXTENSION )
            {
                extensionsExist = TRUE;
            }
            else
            {
            
                cxxtri =  e->type->cxxTypeRefInfo;
                tags = GetTags (e->type, &stoleChoiceTags);

                if (LIST_EMPTY (tags))
                {
                    fprintf (src, "    // ANY Type?\n");
                    fprintf (src, "    case MAKE_TAG_ID (?, ?, ?):\n");
                }
                else
                {
                    tag = (Tag*)FIRST_LIST_ELMT (tags);
                    classStr = Class2ClassStr (tag->tclass);
                    formStr = Form2FormStr (tag->form);

                    if (tag->tclass == UNIV)
                    {
                        codeStr = DetermineCode(tag, NULL, 0);//RWC;Code2UnivCodeStr (tag->code);
                    }
                    else
                    {
                        codeStr = DetermineCode(tag, NULL, 1);
                    }

                    if (tag->form == ANY_FORM)
                    {
                        fprintf (src, "    case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (PRIM), codeStr);
                        fprintf (src, "    case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (CONS), codeStr);
                    }
                    else
                    {
                        fprintf (src, "    case MAKE_TAG_ID (%s, %s, %s):\n", classStr, formStr, codeStr);
                    }

                    /* now decode extra tags/length pairs */
                    AsnListFirst (tags);
                    AsnListNext (tags);
                    elmtLevel = 0;
                    if (stoleChoiceTags)
                    {
                        FOR_REST_LIST_ELMT (tag, tags)
                        {
                            classStr = Class2ClassStr (tag->tclass);

                            formStr = Form2FormStr (tag->form);

                            if (tag->tclass == UNIV)
                                codeStr = DetermineCode(tag, NULL, 0);//RWC;Code2UnivCodeStr (tag->code);
                            else
                                codeStr = DetermineCode(tag, NULL, 1);

                            if (tag->form == ANY_FORM)
                            {
                                fprintf (src, "    case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (PRIM), codeStr);
                                fprintf (src, "    case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (CONS), codeStr);
                            }
                            else
                            {
                                fprintf (src, "    case MAKE_TAG_ID (%s, %s, %s):\n", classStr, formStr, codeStr);
                            }
                        }
                    }
                    else /* didn't steal nested choice's tags */
                    {
                        FOR_REST_LIST_ELMT (tag, tags)
                        {
                            classStr = Class2ClassStr (tag->tclass);
                            codeStr = DetermineCode(tag, NULL, 0);//RWC;Code2UnivCodeStr (tag->code);
                            formStr = Form2FormStr (tag->form);

                            fprintf (src, "      tag = BDecTag (_b, bytesDecoded);\n");
                            if (tag->form == ANY_FORM)
                            {
                                if (tag->tclass == UNIV)
                                {
                                    fprintf (src, "      if ((tag != MAKE_TAG_ID (%s, %s, %s))\n",  classStr, Form2FormStr (PRIM), codeStr);
                                    fprintf (src, "          && (tag != MAKE_TAG_ID (%s, %s, %s)))\n", classStr, Form2FormStr (CONS), codeStr);
                                }
                                else
                                {
                                    fprintf (src, "      if ((tag != MAKE_TAG_ID (%s, %s, %s))\n", classStr, Form2FormStr (PRIM), DetermineCode(tag, NULL, 1));//RWC;tag->code);
                                    fprintf (src, "          && (tag != MAKE_TAG_ID (%s, %s, %s)))\n", classStr, Form2FormStr (CONS), DetermineCode(tag, NULL, 1));//RWC;tag->code);
                                }
                            }
                            else
                            {
                                if (tag->tclass == UNIV)
                                    fprintf (src, "      if (tag != MAKE_TAG_ID (%s, %s, %s))\n", classStr, formStr, codeStr);
                                else
                                    fprintf (src, "      if (tag != MAKE_TAG_ID (%s, %s, %s))\n", classStr, formStr, DetermineCode(tag, NULL, 1));//RWC;tag->code);
                            }

                            fprintf (src, "      {\n");
                        
                            fprintf (src, "        throw InvalidTagException(typeName(), tag, STACK_ENTRY);\n");
                            fprintf (src, "      }\n\n");

                            fprintf (src, "      elmtLen%d = BDecLen (_b, bytesDecoded);\n", ++elmtLevel);
                        }
                    }
                }
                /*
                 * if the choices element is another choice &&
                 * we didn't steal its tags then we must grab
                 * the key tag out of the contained CHOICE
                 */
                if (!stoleChoiceTags && (GetBuiltinType (e->type) == BASICTYPE_CHOICE))
                {
                    fprintf (src, "      tag = BDecTag (_b, bytesDecoded);\n");
                    fprintf (src, "      elmtLen%d = BDecLen (_b, bytesDecoded);\n", ++elmtLevel);
                }

                varName = cxxtri->fieldName;
                /* set choice id for to this elment */
                fprintf (src, "      %s = %s;\n", r->choiceIdFieldName, cxxtri->choiceIdSymbol);

                /* alloc elmt if nec */
                if (cxxtri->isPtr)
                {
                    fprintf (src, "        %s = new %s;\n", varName, cxxtri->className);
                }
                /* decode content */
                tmpTypeId = GetBuiltinType (e->type);
                if (tmpTypeId == BASICTYPE_ANYDEFINEDBY)
                {
                    /*
                     * must check for another EOC for ANYs
                     * since the any decode routines decode
                     * their own first tag/len pair
                     */
                    elmtLevel++;
                    defByNamedType = e->type->basicType->a.anyDefinedBy->link;
                    PrintPySetTypeByCode(defByNamedType, cxxtri, src);

                    fprintf (src, "      %s", varName);
                    if (cxxtri->isPtr)
                        fprintf (src, "->");
                    else
                        fprintf (src, ".");
                    fprintf (src, "B%s (_b, bytesDecoded);\n",  r->decodeBaseName);
                }
                else if (tmpTypeId == BASICTYPE_ANY)
                {
                    elmtLevel++;
                    fprintf (src, "        %s", varName);
                    if (cxxtri->isPtr)
                       fprintf (src, "->");
                    else
                       fprintf (src, ".");
                    fprintf (src, "B%s (_b, bytesDecoded);\n",  r->decodeBaseName);
                }
                else if ( (tmpTypeId == BASICTYPE_OCTETCONTAINING) ||
							     (tmpTypeId == BASICTYPE_BITCONTAINING) )
                {
                   PrintPyDecodeContaining(e->type, r, src);
                }
                else
                {
                    fprintf (src, "        %s", varName);
                    if (cxxtri->isPtr)
                        fprintf (src, "->");
                    else
                        fprintf (src, ".");

                    fprintf (src, "B%s (_b, tag, elmtLen%d, bytesDecoded);\n",  r->decodeContentBaseName, elmtLevel);
                }

                /* decode Eoc (s) */
                for (i = elmtLevel-1; i >= 0; i--)
                {
                    fprintf (src, "      if (elmtLen%d == INDEFINITE_LEN)\n", i);
                    fprintf (src, "        BDecEoc (_b, bytesDecoded);\n");
                }

                fprintf (src, "      break;\n\n");
                FreeTags (tags);
            }
        }

        fprintf (src, "    default:\n");
        fprintf (src, "    {");

        if(extensionsExist)
        {
            fprintf (src, "     AsnAny extAny;\n");
            fprintf (src, "     extension = new AsnExtension;\n");
            fprintf (src, "     choiceId = extensionCid;\n");
            fprintf (src, "     extAny.BDecContent(_b, tag, elmtLen0, bytesDecoded);\n");
            fprintf (src, "     extension->extList.insert( extension->extList.end(), extAny );\n");
        }
        else
        {
            fprintf (src, "        throw InvalidTagException(typeName(), tag, STACK_ENTRY);\n");
        }

        fprintf (src, "      break;\n");
        fprintf (src, "    }\n");
        fprintf (src, "  } // end switch\n");
        fprintf (src, "} // %s::B%s\n\n\n", td->cxxTypeDefInfo->className, r->decodeContentBaseName);
    }
    /* end of code for printing BDecodeContent method */

    /* do BEnc function */
    if (printEncodersG)
    {
        //fprintf (hdr, "  %s		B%s (%s &_b) const;\n", lenTypeNameG, r->encodeBaseName, bufTypeNameG);
        fprintf (src, "%s %s::B%s (%s &_b) const\n", lenTypeNameG, td->cxxTypeDefInfo->className, r->encodeBaseName, bufTypeNameG);
        fprintf (src, "{\n");
        fprintf (src, "    %s l=0;\n", lenTypeNameG);
        fprintf (src, "    l = B%s (_b);\n", r->encodeContentBaseName);

        /* encode each tag/len pair if any */
        FOR_EACH_LIST_ELMT_RVS (tag, choice->tags)
        {
            classStr = Class2ClassStr (tag->tclass);
            formStr = Form2FormStr (CONS);  /* choices are constructed */
            //RWC;tagLen = TagByteLen (tag->code);
            fprintf (src, "    l += BEncConsLen (_b, l);\n");

            if (tag->tclass == UNIV)
            {
                const char* ptr = DetermineCode(tag, &tagLen, 1);
                fprintf (src, "    l += BEncTag%d (_b, %s, %s, %s);\n", tagLen, classStr, formStr, ptr);//RWC;Code2UnivCodeStr (tag->code));
            }
            else
            {
                const char* ptr = DetermineCode(tag, &tagLen, 1);
                fprintf (src, "    l += BEncTag%d (_b, %s, %s, %s);\n", tagLen, classStr, formStr, ptr);//RWC;tag->code);
            }
        }
        fprintf (src, "    return l;\n");
        fprintf (src, "}\n\n");
    }
    /* end of BEnc function */

    /* Do BDec function */
    if (printDecodersG)
    {
        //fprintf (hdr, "  void			B%s (const %s &_b, %s &bytesDecoded);\n", r->decodeBaseName, bufTypeNameG, lenTypeNameG);
        fprintf (src, "void %s::B%s (const %s &_b, %s &bytesDecoded)\n", td->cxxTypeDefInfo->className, r->decodeBaseName, bufTypeNameG, lenTypeNameG);//, envTypeNameG);
        fprintf (src, "{\n");

        if (choice->tags->count > 0)
           fprintf (src, "    FUNC(\"%s::B%s\")\n", td->cxxTypeDefInfo->className, r->decodeBaseName);

        fprintf (src, "    %s elmtLen = 0;\n", lenTypeNameG);
        fprintf (src, "    %s tag;\n", tagTypeNameG);

        /* print extra locals for redundant lengths */
        for (i = 1; (choice->tags != NULL) && (i <= LIST_COUNT (choice->tags)); i++)
        {
            fprintf (src, "    %s extraLen%d = 0;\n", lenTypeNameG, i);
        }
        fprintf (src, "\n");

        /*  decode tag/length pair (s) */
        elmtLevel = 0;
        FOR_EACH_LIST_ELMT (tag, choice->tags)
        {
            classStr = Class2ClassStr (tag->tclass);
            formStr = Form2FormStr (CONS);  /* choices are constructed */
            fprintf (src, "    AsnTag tagId = BDecTag (_b, bytesDecoded);\n");
            fprintf (src, "    if (tagId != ");
            if (tag->tclass == UNIV)
            {
                fprintf (src, "MAKE_TAG_ID (%s, %s, %s))", classStr, formStr, DetermineCode(tag, NULL, 0));//RWC;Code2UnivCodeStr (tag->code));
            }
            else
            {
                fprintf (src, "MAKE_TAG_ID (%s, %s, %s))", classStr, formStr, DetermineCode(tag, NULL, 1));//RWC;tag->code);
            }
            fprintf (src, "    {\n");
            fprintf (src, "          throw InvalidTagException(typeName(), tagId, STACK_ENTRY);\n");
            fprintf (src, "    }\n");
            fprintf (src, "    extraLen%d = BDecLen (_b, bytesDecoded);\n", ++elmtLevel);
        }

        /* decode identifying tag from choice body */
        fprintf (src, "    /*  CHOICEs are a special case - grab identifying tag */\n");
        fprintf (src, "    /*  this allows easier handling of nested CHOICEs */\n");
        fprintf (src, "    tag = BDecTag (_b, bytesDecoded);\n");
        fprintf (src, "    elmtLen = BDecLen (_b, bytesDecoded);\n");
        fprintf (src, "    B%s (_b, tag, elmtLen, bytesDecoded);\n", r->decodeContentBaseName);

        /* grab any EOCs that match redundant, indef lengths */
        for (i = elmtLevel; i > 0; i--)
        {
            fprintf (src, "    if (extraLen%d == INDEFINITE_LEN)\n", i);
            fprintf (src, "        BDecEoc (_b, bytesDecoded);\n");
        }

        fprintf (src, "}\n\n");
    }
    /* end of BDec function */


   if(genPERCode)
   {
    /* do PER Encode, PEnc function */
    if (printEncodersG)
    {
        /****************************/
        /*** FIRST, handle index encoding for PER Choice.  Taking advantage of 
           * the AsnInt class with constraints for the detailed encoding 
           * details.  Declare outside scope of source method for PEnc/PDec. */
        fprintf (src, "class AsnIntChoice_%s: public AsnInt  {\n", td->cxxTypeDefInfo->className);
        fprintf (src, "  public:\n");
        fprintf (src, "  AsnIntChoice_%s(AsnIntType val=0):AsnInt(val){ }\n", 
                         td->cxxTypeDefInfo->className);
        fprintf (src, "  ValueRange* ValueRanges(int &sizeVRList)\n");
        fprintf (src, "  {\n");
        fprintf (src, "  	static ValueRange INT1_ValueRangeList[] = \n");
        fprintf (src, "  		{{ 0, %d, 1 }};\n",
                 choice->basicType->a.choice->count);  /* CONSTANT value for this CHOICE. */
        fprintf (src, "  	sizeVRList = 1;\n");
        fprintf (src, "  	return INT1_ValueRangeList;\n");
        fprintf (src, "  }\n");
        fprintf (src, "};\n\n");

		//RWC;fprintf (hdr, "  AsnLen		PEnc(AsnBufBits &_b, bool bAlign = false) const {AsnLen len; len = 1;return len;};\n");
        //fprintf (hdr, "  %s		P%s (AsnBufBits &_b) const;\n", lenTypeNameG, r->encodeBaseName);
        fprintf (src, "%s %s::P%s (AsnBufBits &_b) const\n", lenTypeNameG, td->cxxTypeDefInfo->className, r->encodeBaseName);
        fprintf (src, "{\n");
        fprintf (src, "    %s l=0;\n", lenTypeNameG);
        fprintf (src, "    FUNC(\"%s::P%s (AsnBufBits &_b)\");\n", td->cxxTypeDefInfo->className, r->encodeBaseName);

        /****************************/
        /*** PERFORM sorting of Choice elements for proper index setting. */
        PrintPyDefCode_PERSort(&ppElementNamedType, &pElementTag, choice->basicType->a.choice);
        fprintf (src, "  AsnIntChoice_%s TmpAsnIntChoice(%d);\n", 
                 td->cxxTypeDefInfo->className, 
                 choice->basicType->a.choice->count);  /* CONSTANT value for this CHOICE. */
        for (ii=0; ii < choice->basicType->a.choice->count; ii++)
        {
            fprintf (src, "   if (%s == %s::%s)\n", r->choiceIdFieldName, 
                  td->cxxTypeDefInfo->className, 
                  ppElementNamedType[ii]->type->cxxTypeRefInfo->choiceIdSymbol);
            fprintf (src, "      TmpAsnIntChoice.Set(%d); // SORTED index value.\n", ii);
        }       // END FOR ii
        free(ppElementNamedType);
        free(pElementTag);

        /*** SETUP specific sorted index value. */
        fprintf (src, "  l = TmpAsnIntChoice.PEnc(_b); // LOAD PER encoded, constrained Choice index value.\n");

        fprintf (src, "  switch (%s)\n", r->choiceIdFieldName);
        fprintf (src, "  {\n");
        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            cxxtri =  e->type->cxxTypeRefInfo;
            fprintf (src, "    case %s:\n", cxxtri->choiceIdSymbol);
      
            varName = cxxtri->fieldName;

            /* encode content */
            tmpTypeId = GetBuiltinType (e->type);
            if (tmpTypeId == BASICTYPE_ANYDEFINEDBY)
            {
               defByNamedType = e->type->basicType->a.anyDefinedBy->link;
               PrintPySetTypeByCode(defByNamedType, cxxtri, src);
            }
            else if (tmpTypeId == BASICTYPE_ANY)
            {
                fprintf (src, "    l += %s", varName);
                if (cxxtri->isPtr)
                    fprintf (src, "->");
                else
                    fprintf (src, ".");

                fprintf (src, "P%s (_b);\n", r->encodeBaseName);
            }
            else if ( (tmpTypeId == BASICTYPE_OCTETCONTAINING) ||
                      (tmpTypeId == BASICTYPE_BITCONTAINING))
            {
               PrintPyPEREncodeContaining(e->type, r, src);
            }
            else
            {
                fprintf (src, "      l += %s", varName);
                if (cxxtri->isPtr)
                    fprintf (src, "->");
                else
                    fprintf (src, ".");

                fprintf (src, "P%s (_b);\n", r->encodeBaseName);
            }
            fprintf (src, "      break;\n\n");
        }
        fprintf (src, "      default:\n");
		  fprintf (src, "         throw EXCEPT(\"Choice is empty\", ENCODE_ERROR);\n");
        fprintf (src, "  } // end switch\n");
        /****************************/

        fprintf (src, "    return l;\n");
        fprintf (src, "}    //%s::P%s(...)\n\n", td->cxxTypeDefInfo->className, r->encodeBaseName);
    }       /* END IF printEncodersG */
    /* end of PEnc function */

    /* Do PDec function */
    if (printDecodersG)
    {
        //fprintf (hdr, "  void			P%s (AsnBufBits &_b, %s &bitsDecoded);\n", r->decodeBaseName, lenTypeNameG);
        fprintf (src, "void %s::P%s (AsnBufBits &_b, %s &bitsDecoded)\n", td->cxxTypeDefInfo->className, r->decodeBaseName, lenTypeNameG);//, envTypeNameG);
        fprintf (src, "{\n");
        fprintf (src, "\tClear();\n");

        /* print extra locals for redundant lengths */
        for (i = 1; (choice->tags != NULL) && (i <= LIST_COUNT (choice->tags)); i++)
        {
            //fprintf (src, "    %s extraLen%d = 0; \n", lenTypeNameG, i);
        }
        fprintf (src, "\n");

        /*  decode tag/length pair (s) */
        elmtLevel = 0;

        /****************************/
        fprintf (src, "  AsnIntChoice_%s TmpAsnIntChoice;\n", td->cxxTypeDefInfo->className);

        /*** SETUP specific sorted index value. */
        fprintf (src, "  TmpAsnIntChoice.PDec(_b, bitsDecoded); // LOAD PER decoded, constrained Choice index value.\n");

        /* decode identifying tag from choice body */
        fprintf (src, "    /*  CHOICEs are a special case - grab identifying tag */\n");
        fprintf (src, "    /*  this allows easier handling of nested CHOICEs */\n");

        /* print local vars */
        /* count max number of extra length var nec
         * by counting tag/len pairs on components of the CHOICE
         */
        varCount = 0;
        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            tmpVarCount = CxxCountVariableLevels (e->type);
            if (tmpVarCount > varCount)
                varCount = tmpVarCount;
        }
 
        /*** PERFORM sorting of Choice elements for proper index setting, then 
             determine actual Choice chosen by the user. */
        PrintPyDefCode_PERSort(&ppElementNamedType, &pElementTag, choice->basicType->a.choice);
        for (ii=0; ii < choice->basicType->a.choice->count; ii++)
        {
            fprintf (src, "   if (TmpAsnIntChoice == %d)\n", ii);
            fprintf (src, "   {\n"); 
            fprintf (src, "      %s = %s::%s;\n", r->choiceIdFieldName, 
                  td->cxxTypeDefInfo->className, 
                  ppElementNamedType[ii]->type->cxxTypeRefInfo->choiceIdSymbol);

            /* Process specific tag - choices always have the key tag decoded */
                e = ppElementNamedType[ii];
                cxxtri =  e->type->cxxTypeRefInfo;

                varName = cxxtri->fieldName;

                /* alloc elmt if nec */
                if (cxxtri->isPtr)
                {
                   fprintf (src, "      %s = new %s;\n", varName, cxxtri->className);
                }
                /* decode content */
                tmpTypeId = GetBuiltinType (e->type);
                if (tmpTypeId == BASICTYPE_ANYDEFINEDBY)
                {
                    /*
                     * must check for another EOC for ANYs
                     * since the any decode routines decode
                     * their own first tag/len pair
                     */
                    elmtLevel++;

                    defByNamedType = e->type->basicType->a.anyDefinedBy->link;
                    PrintPySetTypeByCode(defByNamedType, cxxtri, src);

                    fprintf (src, "      %s", varName);
                    if (cxxtri->isPtr)
                        fprintf (src, "->");
                    else
                        fprintf (src, ".");
                    fprintf (src, "P%s (_b, bitsDecoded);\n",  r->decodeBaseName);
                }
                else if (tmpTypeId == BASICTYPE_ANY)
                {
                    elmtLevel++;
                    fprintf (src, "        %s", varName);
                    if (cxxtri->isPtr)
                       fprintf (src, "->");
                    else
                       fprintf (src, ".");
                    fprintf (src, "P%s (_b, bitsDecoded);\n",  r->decodeBaseName);
                }
                else if ( (tmpTypeId == BASICTYPE_OCTETCONTAINING) ||
							     (tmpTypeId == BASICTYPE_BITCONTAINING) )
                {
                   PrintPyPERDecodeContaining(e->type, r, src);
                }
                else
                {
                    fprintf (src, "      %s", varName);
                    if (cxxtri->isPtr)
                        fprintf (src, "->");
                    else
                        fprintf (src, ".");

                    fprintf (src, "P%s (_b, bitsDecoded);\n",  r->decodeBaseName);
                }


            fprintf (src, "   }  // END if this Choice ID chosen\n"); 
        }       // END FOR ii

        free(ppElementNamedType);
        free(pElementTag);

        fprintf (src, "}   // END %s::P%s(...)\n\n", td->cxxTypeDefInfo->className, r->decodeBaseName);
    }
    /* end of PDec function */
   } /* if genPERCode */


    /* ostream printing routine */
    if (printPrintersG)
    {
        fprintf(src, "void %s::Print(std::ostream& os, unsigned short indent) const\n",
			td->cxxTypeDefInfo->className);
        fprintf(src, "{\n");
        fprintf(src, "\tswitch (choiceId)\n");
        fprintf(src, "\t{\n");

        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            fprintf (src, "\tcase %s:\n", e->type->cxxTypeRefInfo->choiceIdSymbol);

            /* value notation so print the choice elmts field name */
            if (e->fieldName != NULL)
                fprintf(src, "\t\tos << \"%s \";\n", e->fieldName);

            if (e->type->cxxTypeRefInfo->isPtr)
			{
                fprintf(src, "\t\tif (%s)\n", e->type->cxxTypeRefInfo->fieldName);
                fprintf(src, "\t\t\t%s->Print(os, indent);\n",
					e->type->cxxTypeRefInfo->fieldName);
                fprintf(src, "\t\telse\n");
                fprintf(src, "\t\t\tos << \"<CHOICE value is missing>\\n\";\n");
			}
			else
			{
                fprintf(src, "\t\t%s.Print(os, indent);\n",
					e->type->cxxTypeRefInfo->fieldName);
			}

            fprintf (src, "\t\tbreak;\n\n");
        }
        fprintf (src, "\t} // end of switch\n");

        fprintf (src, "} // end of %s::Print()\n\n", td->cxxTypeDefInfo->className);

        //fprintf(hdr, "  void			PrintXML (std::ostream &os, const char *lpszTitle=NULL) const;\n");

        fprintf(src, "void %s::PrintXML (std::ostream &os, const char *lpszTitle) const\n", td->cxxTypeDefInfo->className);
        fprintf(src, "{\n");
        fprintf(src, "    const char *tagName = typeName();\n");
        fprintf(src, "    if (lpszTitle)\n");
        fprintf(src, "        tagName = lpszTitle;");
        fprintf(src, "    os << \"<\" << tagName << \">\";\n");
        fprintf(src, "    switch (choiceId) {\n");

        FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
        {
            fprintf(src, "    case %s:\n",
                     e->type->cxxTypeRefInfo->choiceIdSymbol);

            /* value notation so print the choice elmts field name */
            if (e->type->cxxTypeRefInfo->isPtr)
            {
                fprintf(src, "       if (%s) {\n",
                        e->type->cxxTypeRefInfo->fieldName);
                fprintf(src, "           %s->PrintXML(os", e->type->cxxTypeRefInfo->fieldName);
                if (e->fieldName != NULL)
                   fprintf(src, ",\"%s\");\n", e->fieldName);
                else
                   fprintf(src, ");\n");
                fprintf(src, "      }\n");
            } else
                fprintf(src, "      %s.PrintXML(os, \"%s\");\n",
                        e->type->cxxTypeRefInfo->fieldName, e->fieldName);

            fprintf(src, "      break;\n\n");
        }
        fprintf (src, "    } // end of switch\n");
        fprintf (src, "    os << \"</\" << tagName << \">\";\n");
        fprintf (src, "} // %s::PrintXML\n\n", td->cxxTypeDefInfo->className);
    }
    /* end of Print Method code */
} /* PrintPyChoiceDefCode */

static char *
PyDetermineCode(Tag *tag, int *ptagLen, int bJustIntegerFlag)
{
    static char retstring[256];
    char *codeStr=NULL;
    int iValue=500;     // WILL indicate a problem on source creation...
    memset(retstring, 0, sizeof(retstring));
    if (tag->valueRef == NULL) {
        if (!bJustIntegerFlag) {
            char *univstr = Code2UnivCodeStr (tag->code);
            sprintf(retstring, "asn_base.BERConsts.%s", univstr);
        } else {
            sprintf(retstring, "%d", tag->code); 
        }
        codeStr = retstring;
        if (ptagLen) {
            *ptagLen = TagByteLen(tag->code);
        }
    } else {
        if (tag->valueRef && tag->valueRef->basicValue &&
            tag->valueRef->basicValue->choiceId == BASICVALUE_LOCALVALUEREF &&
            tag->valueRef->basicValue->a.localValueRef &&
            tag->valueRef->basicValue->a.localValueRef->link &&
            tag->valueRef->basicValue->a.localValueRef->link->value &&
            tag->valueRef->basicValue->a.localValueRef->link->value->basicValue)
        {
            if (tag->valueRef->basicValue->a.localValueRef->link->value->basicValue->choiceId == 
                        BASICVALUE_INTEGER)
            {
                iValue = tag->valueRef->basicValue->a.localValueRef->link->
                                                value->basicValue->a.integer;
            }       // IF Integer
            else if (tag->valueRef->basicValue->a.localValueRef->link->value->basicValue->choiceId == 
                        BASICVALUE_LOCALVALUEREF)
            {
                ValueRef *pvalueRef=NULL;
                if (tag->valueRef->basicValue->a.localValueRef->link->value->basicValue->choiceId == BASICVALUE_LOCALVALUEREF)
                {
                    pvalueRef = tag->valueRef->basicValue->a.localValueRef->link->value->basicValue->a.localValueRef;
                    if (pvalueRef->link->value && pvalueRef->link->value->basicValue &&
                        pvalueRef->link->value->basicValue->choiceId == BASICVALUE_INTEGER)
                        iValue = pvalueRef->link->value->basicValue->a.integer;
                }
            }       // END IF Integer/Import
            else
            {
                printf("Tag value type NOT RECOGNIZED; COULD NOT RESOLVE tag integer!\n");
            }
        } else if (tag->valueRef->basicValue->choiceId ==
                   BASICVALUE_IMPORTVALUEREF &&
                   tag->valueRef->basicValue->a.importValueRef &&
                   tag->valueRef->basicValue->a.importValueRef->link &&
                   tag->valueRef->basicValue->a.importValueRef->link->value &&
                   tag->valueRef->basicValue->a.importValueRef->link->value->
                   basicValue && 
                   tag->valueRef->basicValue->a.importValueRef->link->value->
                   basicValue->choiceId == BASICVALUE_INTEGER) {
            iValue = tag->valueRef->basicValue->a.importValueRef->link->
                value->basicValue->a.integer;
        }
        sprintf(retstring, "%d", iValue); 
        codeStr = retstring;
        if (ptagLen) {
            *ptagLen = TagByteLen(iValue);
        }
    }
    return(codeStr);
}

static void
PrintPySeqOrSetDefCode (FILE *src, FILE *hdr, ModuleList *mods, Module *m,
                        PyRules *r ,TypeDef *td, Type *parent ESNACC_UNUSED,
                        Type *seq, int novolatilefuncs ESNACC_UNUSED,
                        int isSequence)
{
    NamedType *e;
    char *classStr;
    char *formStr;
    char *codeStr;
    int tagLen, i=0;
    Tag *tag;
    TagList *tags;
    char *varName;
    CxxTRI *cxxtri=NULL;
    int elmtLevel=0;
    int varCount, tmpVarCount;
    int stoleChoiceTags;
    int inTailOptElmts;
    enum BasicTypeChoiceId tmpTypeId;
    NamedType *defByNamedType;
    NamedType *tmpElmt;

    // DEFINE PER encode/decode tmp vars.
    NamedType **pSeqElementNamedType=NULL;
    int ii;
    int extensionAdditionFound = FALSE;

    fprintf(src, "class %s%s:\n\n", td->cxxTypeDefInfo->className,
            baseClassesG);

    tag = seq->tags->first->data;
    classStr = Class2ClassStr (tag->tclass);
    //formStr = Form2FormStr(CONS);
    codeStr = PyDetermineCode(tag, NULL, (tag->tclass == UNIV) ? 0 : 1);
    fprintf(src, "    BER_CLASS=asn_base.BERConsts.%s\n", classStr);
    fprintf(src, "    BER_FORM=asn_base.BERConsts.BER_CONSTRUCTED_FORM\n\n");
    fprintf(src, "    BER_TAG=%s\n", codeStr);
    /* write out the sequence elmts */

    if (!seq->basicType->a.sequence || !seq->basicType->a.sequence->count) {
        fprintf(stderr, "WARNING: Sequence unknown?\n");
    }

    fprintf(src, "    def typename(self):\n");
    fprintf(src, "        return \"%s\"\n\n", td->cxxTypeDefInfo->className);
    /* constructor */
    fprintf(src, "    def __init__(self");
    FOR_EACH_LIST_ELMT(e, seq->basicType->a.sequence) {
        fprintf(src, ", %s", e->type->cxxTypeRefInfo->fieldName);

        if (e->type->defaultVal) {
            Value *defVal = GetValue(e->type->defaultVal->value);
            switch (ParanoidGetBuiltinType(e->type)) {
            case BASICTYPE_INTEGER:
            case BASICTYPE_ENUMERATED:
                fprintf(src, " = %s(%s)",
                        e->type->cxxTypeRefInfo->className,
                        defVal->basicValue->a.integer);
                break;
            case BASICTYPE_BOOLEAN:
                fprintf(src, " = asn_bool.AsnBool(%s)", 
                        defVal->basicValue->a.boolean == 0 ?
                        "\"False\"" : "\"True\"");
                break;
            case BASICTYPE_BITSTRING:
                fprintf(stderr,
                        "WARNING: unsupported default BIT STRING\n");
                break;
            default:
                fprintf(src, " = %s()\n",
                        e->type->cxxTypeRefInfo->className);
                break;
            } /* end switch */
        } else {
            fprintf(src, " = None");
        }
    }
    fprintf(src, "):\n");

    FOR_EACH_LIST_ELMT(e, seq->basicType->a.sequence) {
        fprintf(src, "        if ");
        fprintf(src, "%s is not None and ",
                    e->type->cxxTypeRefInfo->fieldName);
        fprintf(src, "not isinstance(%s, %s):\n",
                e->type->cxxTypeRefInfo->fieldName,
                e->type->cxxTypeRefInfo->className);
        fprintf(src, "            raise TypeError(\"Expected %s for %s\")\n",
                e->type->cxxTypeRefInfo->className,
                e->type->cxxTypeRefInfo->fieldName);
        fprintf(src, "        else:\n");
        fprintf(src, "            self.%s = %s\n",
                e->type->cxxTypeRefInfo->fieldName,
                e->type->cxxTypeRefInfo->fieldName);
    }

    fprintf(src, "\n\n");

    /* benccontent */
    fprintf(src, "    def BEncContent(self, asnbuf):\n");
    fprintf(src, "        asnlen = 0\n");
    FOR_EACH_LIST_ELMT(e, seq->basicType->a.sequence) {
        fprintf(src, "        ");
        char *indent = "        ";
        if (e->type->cxxTypeRefInfo->isPtr) {
            fprintf(src, "if self.%s is not None:\n",
                    e->type->cxxTypeRefInfo->fieldName);
            indent = "            ";
        } else {
            fprintf(src, "if self.%s is None:\n            "
                    "raise UnboundLocalError('Populate %s before encoding')\n",
                    e->type->cxxTypeRefInfo->fieldName,
                    e->type->cxxTypeRefInfo->fieldName);
        }
        if (e->type->tags->count) {
            fprintf(src, "%s# %d tags to encode\n", indent,
                    e->type->tags->count);
            fprintf(src, "%ssafe_buf = asnbuf\n", indent);
            fprintf(src, "%ssafe_len = asnlen\n", indent);
            fprintf(src, "%sasnlen = 0\n", indent);
            fprintf(src, "%sasnbuf = %s()\n", indent, bufTypeNameG);
        }
        fprintf(src, "%sasnlen += self.%s.BEnc(asnbuf)\n", indent,
                e->type->cxxTypeRefInfo->fieldName);
        /* this will only be entered if the above check for count is true */
        FOR_EACH_LIST_ELMT(tag, e->type->tags) {
            fprintf(src, "%snewbuf = %s()\n", indent, bufTypeNameG);
            fprintf(src, "%stagCode = asn_base.BERConsts.MakeTag(asn_base.BERConsts.%s, asn_base.BERConsts.%s, %d)\n",
                    indent, Class2ClassStr(tag->tclass),
                    Form2FormStr(tag->form), tag->code);
            fprintf(src, "%sasnlen += asn_buffer.BEncDefLen(asnbuf, asnlen)\n",
                    indent);
            fprintf(src, "%sasnlen += asnbuf.PutBufReverse(tagCode)\n",
                    indent);
            fprintf(src, "%snewbuf.PutBuf(asnbuf.buf)\n", indent);
            fprintf(src, "%sasnbuf = newbuf\n", indent);
        }
        if (e->type->tags->count) {
            fprintf(src, "%ssafe_buf.PutBuf(asnbuf.buf)\n", indent);
            fprintf(src, "%sasnbuf = safe_buf\n", indent);
            fprintf(src, "%sasnlen = asnlen + safe_len\n", indent);
        }
    }
    fprintf(src, "        return asnlen\n\n");

    /* bdeccontent */
    fprintf(src, "    def BDecContent(self, asnbuf, length):\n");
    fprintf(src, "        asnlen = 0\n");
    FOR_EACH_LIST_ELMT(e, seq->basicType->a.sequence) {
        int i = 0;
        Tag *lastTag = NULL;
        fprintf(src, "        # tags: %d\n", e->type->tags->count);
        FOR_EACH_LIST_ELMT(tag, e->type->tags) {
            char *formStr = Form2FormStr(tag->form);
            lastTag = tag;
            if (i) {
                fprintf(src, "        bufTag, al = self.BDecTag(asnbuf, asnlen)\n");
                fprintf(src, "        if bufTag != TAG_CODE_EXPECTED:\n");
                fprintf(src, "            raise Exception('ahh: %%x vs %%x' %% (bufTag, TAG_CODE_EXPECTED))\n");
                fprintf(src, "        asnlen += al\n"
                             "        asnbuf.swallow(al)\n");
                fprintf(src, "        tagTotalLen, totalBytesLen = asn_buffer.BDecDefLen(asnbuf)\n");
                fprintf(src, "        asnlen += tagTotalLen\n");
            } else {
                i = 1;
            }
            if (tag->tclass == CNTX)
                formStr = Form2FormStr(CONS);
            fprintf(src,
                    "        TAG_CODE_EXPECTED = asn_base.BERConsts.MakeTag(asn_base.BERConsts.%s, asn_base.BERConsts.%s, %d)\n",
                    Class2ClassStr(tag->tclass), formStr, tag->code);
        }
        if (!e->type->tags->count) {
            fprintf(src,
                    "        TAG_CODE_EXPECTED = asn_base.BERConsts.MakeTag(%s.BER_CLASS,\n"
                    "                                              %s.BER_FORM,\n"
                    "                                              %s.BER_TAG)\n",
                    e->type->cxxTypeRefInfo->className,
                    e->type->cxxTypeRefInfo->className,
                    e->type->cxxTypeRefInfo->className);
        }
        fprintf(src, "        bufTag, al = self.BDecTag(asnbuf, asnlen)\n");
        fprintf(src, "        if bufTag != TAG_CODE_EXPECTED:\n");
        if (!e->type->cxxTypeRefInfo->isPtr)
            fprintf(src, "            raise Exception('Unexpected: %%x; expected: %%x' % (bufTag, TAG_CODE_EXPECTED))\n");
        else
            fprintf(src, "            pass\n");
        fprintf(src, "        else:\n");
        if (e->type->tags->count == 1 && lastTag && lastTag->tclass == CNTX) {
            fprintf(src, "            asnbuf.swallow(al)\n");
            fprintf(src, "            tagTotalLen, totalBytesLen = asn_buffer.BDecDefLen(asnbuf)\n");
            fprintf(src, "            asnlen += tagTotalLen\n");
        }
        fprintf(src, "            self.%s = %s()\n",
                e->type->cxxTypeRefInfo->fieldName,
                e->type->cxxTypeRefInfo->className);
        fprintf(src, "            asnlen += self.%s.BDec(asnbuf, length)\n",
                e->type->cxxTypeRefInfo->fieldName);
    }
    fprintf(src, "        return asnlen\n\n");

    fprintf(src, "    def __str__(self):\n");
    fprintf(src, "        s = \"%%s = {\" %% self.typename()\n");
    FOR_EACH_LIST_ELMT(e, seq->basicType->a.sequence) {
        fprintf(src, "        s += \"%s = \"\n",
                e->type->cxxTypeRefInfo->fieldName);
        fprintf(src, "        if self.%s is not None:\n",
                e->type->cxxTypeRefInfo->fieldName);
        fprintf(src, "            s += str(self.%s)\n",
                e->type->cxxTypeRefInfo->fieldName);
        fprintf(src, "        else:\n");
        fprintf(src, "            s += \"None\"\n");
        fprintf(src, "        s += \"\\n\"\n");
    }
    fprintf(src, "        s += \"}\"\n");
    fprintf(src, "        return s;\n");
}


static void
PrintPySeqDefCode (FILE *src, FILE *hdr, ModuleList *mods, Module *m,
                    PyRules *r ,TypeDef *td, Type *parent ESNACC_UNUSED,
                    Type *seq, int novolatilefuncs ESNACC_UNUSED)
{

    PrintPySeqOrSetDefCode (src, hdr, mods, m, r, td, parent, seq,
                            novolatilefuncs, 1);

} /* PrintPySeqDefCode */


static void
PrintPySetDefCode (FILE *src, FILE *hdr, ModuleList *mods, Module *m,
                    PyRules *r, TypeDef *td, Type *parent ESNACC_UNUSED,
                    Type *set, int novolatilefuncs ESNACC_UNUSED)
{
    PrintPySeqOrSetDefCode (src, hdr, mods, m, r, td, parent, set,
                            novolatilefuncs, 0);

} /* PrintPySetDefCode */


/*
 * PIERCE added 8-21-2001 template code to handle SET/SEQ OF
 *    
 */
static void
PrintPyListClass(FILE *src, TypeDef *td, Type *lst, Module* m,
                 ModuleList *mods)
{  
   struct NamedType p_etemp;
   NamedType* p_e;
   char typeNameStr[256];
   char *lcn; /* list class name */
   char *ecn; /* (list) elmt class name */
   char pcvarname[100];

   p_e=&p_etemp;
   p_e->type=lst->basicType->a.setOf;

   ecn = lst->basicType->a.setOf->cxxTypeRefInfo->className;
   lcn = td->cxxTypeDefInfo->className;

   fprintf(src, "class %s(asn_list.", lcn);
   switch (lst->basicType->choiceId) {
   case BASICTYPE_SEQUENCEOF:
       fprintf(src, "AsnSequenceOf");
	    break;
   case BASICTYPE_SETOF:
       fprintf(src, "AsnSetOf");
       break;

   default:
       break;
   }
   fprintf(src, "):\n");
   fprintf(src, "    def __init__(self, elemts=None, expected=None):\n");
   fprintf(src, "        if expected is None:\n");
   fprintf(src, "            expected = %s\n", ecn);
   fprintf(src, "        asn_list.%s.__init__(self, elemts, expected)\n\n",
           (lst->basicType->choiceId == BASICTYPE_SEQUENCEOF) ?
           "AsnSequenceOf" : "AsnSetOf");

   fprintf(src, "    def typename(self):\n");
   fprintf(src, "        return \"%s\"\n\n", lcn);

	// JKG--added functionality for sequence of and set of constraints
	if (td->type->subtypes != NULL)
	{
        /*Subtype* s_type;*/
        /*s_type = (Subtype*)td->type->subtypes->a.single->a.sizeConstraint->a.or->last->data;*/
		/* Only single size constraints that are themselves single are supported */
		if ((td->type->subtypes->choiceId == SUBTYPE_SINGLE) &&
			(td->type->subtypes->a.single->choiceId == SUBTYPEVALUE_SIZECONSTRAINT) &&
			(td->type->subtypes->a.single->a.sizeConstraint->choiceId == SUBTYPE_SINGLE))
		{
#if 0
			PrintPySetOfSizeConstraint(src, 
				td->type->subtypes->a.single->a.sizeConstraint->a.single,
				m, td->type);
#endif
		}
		else
		{
			PrintErrLoc(m->asn1SrcFileName, (long)td->type->lineNo);
			fprintf(errFileG, "ERROR - unsupported constraint\n");
		}
	}

}

static void
PrintPySetOfDefCode PARAMS ((src, mods, m, r, td, parent, setOf, novolatilefuncs),
    FILE *src _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    PyRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *setOf _AND_
    int novolatilefuncs)
{
	// Get rid of warnings
	novolatilefuncs = novolatilefuncs;
	parent = parent;
	m = m;
	r = r;
	mods = mods;
	src = src;

    /* do class */
	PrintPyListClass(src, td, setOf, m, mods);

} /* PrintPySetOfDefCode */


static void
PrintPyTypeDefCode PARAMS ((src, hdr, mods, m, r, td, novolatilefuncs),
                           FILE *src _AND_
                           FILE *hdr _AND_
                           ModuleList *mods _AND_
                           Module *m _AND_
                           PyRules *r _AND_
                           TypeDef *td _AND_
                           int novolatilefuncs)
{
    switch (td->type->basicType->choiceId) {
        case BASICTYPE_BOOLEAN:  /* library type */
        case BASICTYPE_REAL:  /* library type */
        case BASICTYPE_OCTETSTRING:  /* library type */
        case BASICTYPE_NULL:  /* library type */
        case BASICTYPE_EXTERNAL:		/* library type */
        case BASICTYPE_OID:  /* library type */
        case BASICTYPE_RELATIVE_OID:
        case BASICTYPE_INTEGER:  /* library type */
        case BASICTYPE_BITSTRING:  /* library type */
        case BASICTYPE_ENUMERATED:  /* library type */
        case BASICTYPE_NUMERIC_STR:  /* 22 */
        case BASICTYPE_PRINTABLE_STR: /* 23 */
        case BASICTYPE_UNIVERSAL_STR: /* 24 */
        case BASICTYPE_IA5_STR:      /* 25 */
        case BASICTYPE_BMP_STR:      /* 26 */
        case BASICTYPE_UTF8_STR:     /* 27 */
        case BASICTYPE_UTCTIME:      /* 28 tag 23 */
        case BASICTYPE_GENERALIZEDTIME: /* 29 tag 24 */
        case BASICTYPE_GRAPHIC_STR:     /* 30 tag 25 */
        case BASICTYPE_VISIBLE_STR:     /* 31 tag 26  aka ISO646String */
        case BASICTYPE_GENERAL_STR:     /* 32 tag 27 */
        case BASICTYPE_OBJECTDESCRIPTOR:	/* 33 tag 7 */
        case BASICTYPE_VIDEOTEX_STR:	/* 34 tag 21 */
        case BASICTYPE_T61_STR:			/* 35 tag 20 */
            PrintPySimpleDef(hdr, src, r, td);
            break;
        case BASICTYPE_SEQUENCEOF:  /* list types */
        case BASICTYPE_SETOF:
            PrintPySetOfDefCode(src, mods, m, r, td, NULL, td->type,
                                novolatilefuncs);
            break;
        case BASICTYPE_IMPORTTYPEREF:  /* type references */
        case BASICTYPE_LOCALTYPEREF:
            /*
             * if this type has been re-tagged then
             * must create new class instead of using a typedef
             */
            PrintPySimpleDef(hdr, src, r, td);
            break;
        case BASICTYPE_CHOICE:
            PrintPyChoiceDefCode (src, hdr, mods, m, r, td, NULL, td->type, novolatilefuncs);
            break;
        case BASICTYPE_SET:
            PrintPySetDefCode (src, hdr, mods, m, r, td, NULL, td->type, novolatilefuncs);
            break;
        case BASICTYPE_SEQUENCE:
            PrintPySeqDefCode (src, hdr, mods, m, r, td, NULL, td->type, novolatilefuncs);
            break;
        case BASICTYPE_COMPONENTSOF:
        case BASICTYPE_SELECTION:
        case BASICTYPE_UNKNOWN:
        case BASICTYPE_MACRODEF:
        case BASICTYPE_MACROTYPE:
        case BASICTYPE_ANYDEFINEDBY:  /* ANY types */
        case BASICTYPE_ANY:
            /* do nothing */
            break;
        default:
				/* TBD: print error? */
				break;
    }
} /* PrintPyTypeDefCode */

void
PrintPyCode PARAMS ((src, hdr, if_META (printMeta COMMA meta COMMA meta_pdus COMMA)
					 mods, m, r, longJmpVal, printTypes, printValues,
					 printEncoders, printDecoders, printPrinters, printFree
					 if_TCL (COMMA printTcl), novolatilefuncs),
    FILE *src _AND_
    FILE *hdr _AND_
    if_META (MetaNameStyle printMeta _AND_)
    if_META (const Meta *meta _AND_)
    if_META (MetaPDU *meta_pdus _AND_)
    ModuleList *mods _AND_
    Module *m _AND_
    PyRules *r _AND_
    long longJmpVal _AND_
    int printTypes _AND_
    int printValues _AND_
    int printEncoders _AND_
    int printDecoders _AND_
    int printPrinters _AND_
    int printFree
    if_TCL (_AND_ int printTcl) _AND_
    int novolatilefuncs)
{
	Module *currMod;
    AsnListNode *currModTmp;
    TypeDef *td;
    ValueDef *vd;


    printTypesG = printTypes;
    printEncodersG = printEncoders;
    printDecodersG = printDecoders;
    printPrintersG = printPrinters;
    printFreeG = printFree;

#if META
    printMetaG = printMeta;
    meta_pdus_G = meta_pdus;
#if TCL
    printTclG = printTcl;
#endif /* TCL */
#endif /* META */

    PrintSrcComment(src, m);
    PrintSrcIncludes(src);

    FOR_EACH_LIST_ELMT (currMod, mods) {
		if (!strcmp(m->cxxHdrFileName, currMod->cxxHdrFileName)) {
            ImportModuleList *ModLists;
            ImportModule *impMod;
            char *ImpFile = NULL;
            ModLists = currMod->imports;
            currModTmp = mods->curr;
            FOR_EACH_LIST_ELMT(impMod, ModLists) {
                ImpFile = GetImportFileName(impMod->modId->name, mods);
                if (ImpFile != NULL)
                    fprintf(src, "import %s\n", ImpFile);
                if (impMod->moduleRef == NULL)
                    impMod->moduleRef = GetImportModuleRef(impMod->modId->name, mods);
            }
            mods->curr = currModTmp;
        }
    }
    fprintf(src, "\n");

    if (printValues) {
        fprintf(src, "# value defs\n\n");
        FOR_EACH_LIST_ELMT (vd, m->valueDefs) {
#if 0
            PrintPyValueDef(src, r, vd);
#endif
        }
        fprintf(src, "\n");
    }

    fprintf(src, "# class member definitions:\n\n");
    PrintPyAnyCode (src, hdr, r, mods, m);

    FOR_EACH_LIST_ELMT (td, m->typeDefs) {
        PrintPyTypeDefCode(src, hdr, mods, m, r, td, novolatilefuncs);
    }

} /* PrintPyCode */


void PrintPyEncodeContaining(Type *t, PyRules *r, FILE *src)
{
   fprintf(src, "    l += %s", t->cxxTypeRefInfo->fieldName);
   if (t->cxxTypeRefInfo->isPtr)
      fprintf(src, "->");
   else
      fprintf(src, ".");

   fprintf(src, "B%s(_b);\n", r->encodeBaseName);

   /* If this is a BITSTRING CONTAINING encode a NULL octet for the unused 
    * bits
    */
   if (t->basicType->choiceId == BASICTYPE_BITCONTAINING)
   {
      fprintf(src,"    _b.PutByteRvs((char) 0 ); //encode 0 for unused bits\n");
      fprintf(src,"    l++;\n");
   }
}

void PrintPyDecodeContaining(Type *t, PyRules *r, FILE *src)
{
   NamedType *defByNamedType;

   /* Encode Content of contained type */
   if (t->basicType->a.stringContaining->basicType->choiceId == 
			 BASICTYPE_ANYDEFINEDBY)
   {
      defByNamedType = 
			  t->basicType->a.stringContaining->basicType->a.anyDefinedBy->link;
      PrintPySetTypeByCode(defByNamedType, t->cxxTypeRefInfo, src);
   }

   if (t->basicType->choiceId == BASICTYPE_BITCONTAINING)
   {
      fprintf(src,"\n");
      fprintf(src,"    // Decode unused bits and make sure it's 0\n");
      fprintf(src,"    char unusedBits;\n");
      fprintf(src,"    unusedBits = _b.GetByte();\n");
      fprintf(src,"    seqBytesDecoded++;\n");
      fprintf(src,"    if (unusedBits != '0x0')\n");
      fprintf(src,"      throw DecodeException(STACK_ENTRY);\n");
      fprintf(src,"\n");
   }

   fprintf (src, "    %s", t->cxxTypeRefInfo->fieldName);
   if (t->cxxTypeRefInfo->isPtr)
      fprintf (src, "->");
   else
      fprintf (src, ".");

   fprintf (src, "B%s (_b, seqBytesDecoded);\n",  r->decodeBaseName);
}


void PrintPyPEREncodeContaining(Type *t, PyRules *r, FILE *src)
{
   fprintf(src, "    l += %s", t->cxxTypeRefInfo->fieldName);
   if (t->cxxTypeRefInfo->isPtr)
      fprintf(src, "->");
   else
      fprintf(src, ".");

   fprintf(src, "P%s(_b);\n", r->encodeBaseName);

   /* If this is a BITSTRING CONTAINING encode a NULL octet for the unused 
    * bits
    */
   if (t->basicType->choiceId == BASICTYPE_BITCONTAINING)
   {
      fprintf(src,"    unsigned char _tmp[] = {0x00};\n");
      fprintf(src,"    _b.PutBits(tmp , 8); //encode 0 for unused bits\n");
      fprintf(src,"    l++;\n");
   }
}

void PrintPyPERDecodeContaining(Type *t, PyRules *r, FILE *src)
{
   NamedType *defByNamedType;

   /* Encode Content of contained type */
   if (t->basicType->a.stringContaining->basicType->choiceId == 
			 BASICTYPE_ANYDEFINEDBY)
   {
      defByNamedType = 
			  t->basicType->a.stringContaining->basicType->a.anyDefinedBy->link;
      PrintPySetTypeByCode(defByNamedType, t->cxxTypeRefInfo, src);
   }

   if (t->basicType->choiceId == BASICTYPE_BITCONTAINING)
   {
      fprintf(src,"\n");
      fprintf(src,"    // Decode unused bits and make sure it's 0\n");
      fprintf(src,"    unsigned char* unusedBits;\n");
      fprintf(src,"    unusedBits = _b.GetBits(8);\n");
      fprintf(src,"    bitsDecoded++;\n");
      fprintf(src,"    if (unusedBits[0] != '0x0')\n");
      fprintf(src,"      throw DecodeException(STACK_ENTRY);\n");
      fprintf(src,"\n");
   }

   fprintf (src, "    %s", t->cxxTypeRefInfo->fieldName);
   if (t->cxxTypeRefInfo->isPtr)
      fprintf (src, "->");
   else
      fprintf (src, ".");

   fprintf (src, "P%s (_b, bitsDecoded);\n",  r->decodeBaseName);
}


void PrintPySetTypeByCode(NamedType *defByNamedType, CxxTRI *cxxtri, FILE *src)
{
     char *varName = cxxtri->fieldName;

     if (GetBuiltinType (defByNamedType->type) == BASICTYPE_OID)
     {
         fprintf (src, "    %s", varName);
         if (cxxtri->isPtr)
            fprintf (src, "->");
         else
            fprintf (src, ".");

         fprintf (src, "SetTypeByOid (");
         if (defByNamedType->type->cxxTypeRefInfo->isPtr)
             fprintf (src, " *");
         fprintf (src, "%s);\n", defByNamedType->type->cxxTypeRefInfo->fieldName);
     }
     else if (GetBuiltinType (defByNamedType->type) == BASICTYPE_INTEGER)
     {
         fprintf (src, "    %s", varName);
         if (cxxtri->isPtr)
            fprintf (src, "->");
         else
            fprintf (src, ".");

         fprintf (src, "SetTypeByInt (");
         if (defByNamedType->type->cxxTypeRefInfo->isPtr)
             fprintf (src, " *");
         fprintf (src, "%s);\n", defByNamedType->type->cxxTypeRefInfo->fieldName);
     }
     else if (GetBuiltinType (defByNamedType->type) == BASICTYPE_CHOICE)
     {
        NamedType *nt;
        Type      *t = GetType(defByNamedType->type);

        if (defByNamedType->type->cxxTypeRefInfo->isPtr)
            fprintf(src, "  switch (%s->choiceId)\n", defByNamedType->type->cxxTypeRefInfo->fieldName);
        else
            fprintf(src, "  switch (%s.choiceId)\n", defByNamedType->type->cxxTypeRefInfo->fieldName);
        fprintf(src, "  {\n");

        FOR_EACH_LIST_ELMT(nt, t->basicType->a.choice)
        {
           fprintf(src, "   case %s::%sCid:\n", defByNamedType->type->cxxTypeRefInfo->className, nt->fieldName);
           if (nt->type->basicType->choiceId == BASICTYPE_INTEGER ||
               nt->type->basicType->choiceId == BASICTYPE_ENUMERATED)
           {
              fprintf (src, "      %s", varName);
              if (cxxtri->isPtr)
                 fprintf (src, "->");
              else
                 fprintf (src, ".");

              if (defByNamedType->type->cxxTypeRefInfo->isPtr)
                  fprintf(src, "SetTypeByInt(*%s->%s);\n", defByNamedType->type->cxxTypeRefInfo->fieldName, nt->fieldName);
              else
                  fprintf(src, "SetTypeByInt(*%s.%s);\n", defByNamedType->type->cxxTypeRefInfo->fieldName, nt->fieldName);
           }
           else
           {
              fprintf (src, "      %s", varName);
              if (cxxtri->isPtr)
                 fprintf (src, "->");
              else
                 fprintf (src, ".");

              if (defByNamedType->type->cxxTypeRefInfo->isPtr)
                  fprintf(src, "SetTypeByOid(*%s->%s);\n", defByNamedType->type->cxxTypeRefInfo->fieldName, nt->fieldName);
              else
                  fprintf(src, "SetTypeByOid(*%s.%s);\n", defByNamedType->type->cxxTypeRefInfo->fieldName, nt->fieldName);
           }
           fprintf(src, "      break;\n");

        }
        fprintf(src, "  }\n");
     }
}


static char *
LookupNamespace PARAMS ((t, mods),
    Type *t _AND_
    ModuleList *mods)
{
    char *pszNamespace=NULL;
    Module *mTmp=NULL;
    TypeDef *ptTmp=NULL;
    BasicType *pbtTmp2=NULL;

    //RWC; HANDLE namespace designations of specific modules on declarations,
    //      if necessary.  (May have to lookup module name to get namespace).
    pbtTmp2 = t->basicType;
    if (pbtTmp2->choiceId == BASICTYPE_SEQUENCEOF || 
        pbtTmp2->choiceId == BASICTYPE_SETOF)
        pbtTmp2 = pbtTmp2->a.sequenceOf->basicType;  // Burrow 1 more layer down for SequenceOf/SetOf
    if (pbtTmp2->choiceId == BASICTYPE_IMPORTTYPEREF)
    {                     // RWC; IF IMPORTED, then we need to check for 
                          //       optional namespace designation (only in .h)
            FOR_EACH_LIST_ELMT (mTmp, mods)
            {
                ptTmp = LookupType(mTmp->typeDefs, 
                          pbtTmp2->a.importTypeRef->typeName); //WHAT we are looking for...
                if (ptTmp != NULL)
                    break;      //FOUND the MODULE that contains our defninition...
            }       // END FOR each module.
            if (ptTmp != NULL && mTmp != NULL && mTmp->namespaceToUse)  // FOUND our MODULE...
            {
                pszNamespace = mTmp->namespaceToUse;    // DO NOT DELETE...
            }    
            //LookupType PARAMS ((typeDefList, typeName),
    }           // IF BASICTYPE_IMPORTTYPEREF

    return(pszNamespace);
}       /* END LookupNamespace(...)*/


void PrintPySeqSetPrintFunction(FILE* src, FILE* hdr, MyString className,
								 BasicType *pBasicType)
{
    int allOpt;
	int inTailOptElmts;
	NamedTypeList* pElmtList;
	NamedType *e;

	fprintf(src, "void %s::Print(std::ostream& os, unsigned short indent) const\n",
		className);
	fprintf(src, "{\n");

	if (pBasicType->choiceId == BASICTYPE_SEQUENCE)
	{
		fprintf(src, "\tos << \"{ -- SEQUENCE --\" << std::endl;\n");
		pElmtList = pBasicType->a.sequence;
	}
	else if (pBasicType->choiceId == BASICTYPE_SET)
	{
		fprintf(src, "\tos << \"{ -- SET --\" << std::endl;\n");
		pElmtList = pBasicType->a.set;
	}
	else
		abort();

	allOpt = AllElmtsOptional(pElmtList);
	if (allOpt)
		fprintf(src, "\tint nonePrinted = true;\n");

	fprintf(src, "++indent;\n\n");

	FOR_EACH_LIST_ELMT (e, pElmtList)
	{
		inTailOptElmts = IsTailOptional(pElmtList);

		if (e->type->cxxTypeRefInfo->isPtr)
		{
			fprintf(src, "\tif (%s (%s))\n",
				e->type->cxxTypeRefInfo->optTestRoutineName,
				e->type->cxxTypeRefInfo->fieldName);
			fprintf (src, "\t{\n");

			if (allOpt)
			{
				if (e != FIRST_LIST_ELMT (pElmtList))
				{
					fprintf(src, "\t\tif (!nonePrinted)\n");
					fprintf(src, "\t\t\tos << \",\" << std::endl;\n");
				}
				fprintf(src, "\t\tnonePrinted = false;\n");
			}
			/* cannot be first elmt ow allOpt is true */
			else if (inTailOptElmts) 
				fprintf (src, "\t\tos << \",\"<< std::endl;\n");

			fprintf(src, "\t\tIndent(os, indent);\n");

			if (e->fieldName != NULL)
				fprintf(src, "\t\tos << \"%s \";\n", e->fieldName);

			fprintf(src, "\t\t%s->Print(os, indent);\n",
				e->type->cxxTypeRefInfo->fieldName);

			fprintf(src, "\t}\n");
		}
		else
		{
			fprintf(src, "\tIndent(os, indent);\n");

			if (e->fieldName != NULL)
				fprintf(src, "\tos << \"%s \";\n", e->fieldName);

			fprintf(src, "\t%s.Print(os, indent);\n",
				e->type->cxxTypeRefInfo->fieldName);

			if (e != LAST_LIST_ELMT (pElmtList))
				fprintf(src, "\tos << ',' << std::endl;\n");
		}

		fprintf (src, "\n");

		if (e == LAST_LIST_ELMT (pElmtList))
			fprintf(src, "\tos << std::endl;\n");
	}

	fprintf(src, "\t--indent;\n");
	fprintf(src, "\tIndent(os, indent);\n");
	fprintf(src, "\tos << \"}\\n\";\n");
	fprintf (src, "} // end of %s::Print()\n\n", className);
} /* end of PrintPySeqSetPrintFunction() */


/*
 * RWC;   */
static void
PrintPyDefCode_SetSeqPEREncode (FILE *src, FILE *hdr, PyRules *r, TypeDef *td, 
    NamedType **pSetElementNamedType,
    int iElementCount)      /* IN, ELEMENT Count to process in array*/
{
	NamedType *e;
	char *varName;
	CxxTRI *cxxtri=NULL;
	enum BasicTypeChoiceId tmpTypeId;
	NamedType *defByNamedType;
    int extensionsExist = FALSE;
	
	// DEFINE PER encode/decode tmp vars.
	int ii=0;
	long lOptional_Default_ElmtCount=0;
    const char* tabAndlenVar = "\tl";
		
	//fprintf(hdr, "\t%s\t\tP%s(AsnBufBits &_b) const;\n", lenTypeNameG,
    //        r->encodeBaseName);
      /*RWC; {AsnLen len; len = 1;return len;};\n");
		RWC; MUST sort the results by tag; usually explicit except for 
		RWC;  untagged Choices which can be nested.  We must determine which 
		RWC;  tag can go 1st from any Choice, potentially nested. 
	RWC;  (FORWARD ENCODING FOR PER!)*/
	
	fprintf(src, "%s %s::P%s(AsnBufBits &_b) const\n", lenTypeNameG,
		td->cxxTypeDefInfo->className, r->encodeBaseName);
	fprintf(src, "{\n\t%s l = 0;\n", lenTypeNameG);

	/* SECOND, determine ahead of time the bit count for OPTIONAL/DEFAULT values. */
	for (ii=0; ii < iElementCount; ii++)
	{
		e = pSetElementNamedType[ii];
		
		//RWC; ALSO, count any OPTIONAL/DEFAULT ASN.1 elements.
		if ( (e->type->optional || e->type->defaultVal != NULL) && (!e->type->extensionAddition))
			lOptional_Default_ElmtCount++;
	}
	
	/* NEXT, decode this number of bits, if any, to determine the 
	presence/absence of OPTIONAL/DEFAULT elements.*/
	if (lOptional_Default_ElmtCount)
	{	/* NOW, load PER encoding flag to indicate what OPTIONAL/DEFAULT
		fields are actually present.*/
		/* FOR PER ENCODING OF Set, we must load a bitfield of length 
		"lOptional_Default_ElmtCount", indicating presence of optional 
		or default field data. */
		int iOptional_Default_ElementIndex=0;
		fprintf(src, "\n\t// Build and encode preamble");
		fprintf(src, "\n\tAsnBits SnaccOptionalDefaultBits(%ld);\n", lOptional_Default_ElmtCount);
		for (ii=0; ii < iElementCount; ii++)
		{
			e = pSetElementNamedType[ii];
			if ( (e->type->optional || e->type->defaultVal != NULL) && (!e->type->extensionAddition))
			{
				fprintf (src, "\tif (%s != NULL)\n",
					e->type->cxxTypeRefInfo->fieldName);
				fprintf (src, "\t\tSnaccOptionalDefaultBits.SetBit(%d);\n",
					iOptional_Default_ElementIndex++);
			}		/* END IF OPTIONAL/DEFAULT */
		}	/* END FOR each element. */
		fprintf (src, "\t_b.PutBits(SnaccOptionalDefaultBits.data(), %ld);\n",
			lOptional_Default_ElmtCount);
		fprintf (src, "\tl += %ld;\n", lOptional_Default_ElmtCount);
       
	}		/* END IF lOptional_Default_ElmtCount */
	
	/* NEXT, process each element of the Set/Sequence for decoding. */
	fprintf(src, "\n\t// Encode the elements of the SEQUENCE\n");
	for (ii=0; ii < iElementCount; ii++)
	{
		e = pSetElementNamedType[ii];
        if(!e->type->extensionAddition)
        {


		    if ( (e->type->optional || e->type->defaultVal != NULL) && (!e->type->extensionAddition))
		    {
			    fprintf(src, "\tif (%s != NULL)\t// Optional OR Default\n",
				    e->type->cxxTypeRefInfo->fieldName);
			    tabAndlenVar = "\t\tl";
		    }
		    
		    cxxtri =  e->type->cxxTypeRefInfo;
		    varName = cxxtri->fieldName;
		    
		    /* encode tag(s), not UNIV but APL, CNTX or PRIV tags ONLY for PER */
		    
		    /*RWC;TBD; UPDATE to reflect PER encoding rules for encoding length, no tags
		    RWC;TBD;  unless explicit (probably need to write a PER version, checking type).*/
		    
		    //RWC;NOT FOR PER;PrintPyTagAndLenEncodingCode (src, td, e->type, "l", "(*iBuf)");
		    
		    /* encode content */
		    tmpTypeId = GetBuiltinType (e->type);
		    if (tmpTypeId == BASICTYPE_ANYDEFINEDBY)
		    {
			    //RWC;TBD; we may have to investigate individual types here to 
			    //RWC;TBD;	restrict which codes are printed for PER...
			    defByNamedType = e->type->basicType->a.anyDefinedBy->link;
			    PrintPySetTypeByCode(defByNamedType, cxxtri, src);
			    
			    fprintf(src, "%s += %s", tabAndlenVar, varName);
			    if (cxxtri->isPtr)
				    fprintf (src, "->");
			    else
				    fprintf (src, ".");
			    fprintf (src, "P%s(_b);\n", r->encodeBaseName);
		    }
		    else if (tmpTypeId == BASICTYPE_ANY)
		    {
			    //RWC;NOTE:  we will assume here that the ANY buffer is already
			    //RWC;NOTE:    properly PER encoder; we have no way of checking.
			    fprintf(src, "%s += %s", tabAndlenVar, varName);
			    if (cxxtri->isPtr)
				    fprintf (src, "->");
			    else
				    fprintf (src, ".");
			    
			    fprintf (src, "P%s(_b);\n", r->encodeBaseName);
		    }
		    else if ( (tmpTypeId == BASICTYPE_OCTETCONTAINING) ||
			    (tmpTypeId == BASICTYPE_BITCONTAINING))
		    {
			    PrintPyPEREncodeContaining(e->type, r, src);
			    //RWC;TBD; THIS CALL WILL NOT UPDATE THE COUNT VALUE PROPERLY; must reflect
			    //RWC;TBD;	PER encoding forward, l+=, instead of l=
		    }
		    else
		    {
			    fprintf(src, "%s += %s", tabAndlenVar, varName);
			    if (cxxtri->isPtr)
				    fprintf (src, "->");
			    else
				    fprintf (src, ".");
			    
			    fprintf (src, "P%s(_b);\n", r->encodeBaseName);/*RWC;r->encodeContentBaseName);*/
		    }
        }
        else
        {
            extensionsExist = TRUE;
        }
	}			/* END FOR iElementCount */
	

    if(extensionsExist)
    {
        fprintf (src, " \t/*   WARNING:  PER does not yet support extensibility */\n");
    }

	fprintf(src, "\n\treturn l;\n");
	fprintf(src, "}\t// %s::P%s\n\n\n", td->cxxTypeDefInfo->className,
		r->encodeBaseName);
} /* END PrintPyDefCode_SetSeqPEREncode(...) */


/*
 * RWC;  This method only handles the entire routine decode operations for both Set and Sequence PER Decode.
 * element decodes, not the wrapping logic. */
static void
PrintPyDefCode_SetSeqPERDecode (FILE *src, FILE *hdr, PyRules *r, TypeDef *td, 
    NamedType **pSetElementNamedType,
    int iElementCount)      /* IN, ELEMENT Count to process in arrays */
{
	NamedType *e;
	char *varName;
	CxxTRI *cxxtri=NULL;
	int elmtLevel=0;
	int varCount, tmpVarCount;
    enum BasicTypeChoiceId tmpTypeId;
	NamedType *defByNamedType;

    // DEFINE PER encode/decode tmp vars.
	int ii=0;
	int iOptional_Default_ElementIndex=0;
	long lOptional_Default_ElmtCount=0;
	
	if (pSetElementNamedType == NULL)
	{
		printf("****** PrintPyDefCode_SetSeqPERDecode: MUST HAVE PER Encoders as well as PER Decoders! *****\n");
		return;
	}
	
	/***RWC; PERFORM PDec operations first... */
	//fprintf(hdr, "\tvoid\t\tP%s(AsnBufBits& _b, %s& bitsDecoded);\n\n",
    //r->decodeBaseName, lenTypeNameG);
	
	fprintf(src, "void %s::P%s(AsnBufBits& _b, %s& bitsDecoded)\n",
		td->cxxTypeDefInfo->className, r->decodeBaseName, lenTypeNameG);
	fprintf(src, "{\n");
	fprintf(src, "\tClear();\n");

	/* count max number of extra length var nec */
	varCount = 0;
	
    /*	decode tag/length pair (s) */
	elmtLevel = 0;
	
	for (ii=0; ii < iElementCount; ii++)
		//FOR_EACH_LIST_ELMT (e, set->basicType->a.set)
	{
		e = pSetElementNamedType[ii];
		tmpVarCount = CxxCountVariableLevels (e->type);
		if (tmpVarCount > varCount)
			varCount = tmpVarCount;
		if ( (e->type->optional || e->type->defaultVal != NULL) && (!e->type->extensionAddition))
		{
			lOptional_Default_ElmtCount++;
		}
	}
	
	/* NEXT, decode this number of bits, if any, to determine the 
	presence/absence of OPTIONAL/DEFAULT elements.	MUST BE DONE BEFORE 
	TAGs.*/
	if (lOptional_Default_ElmtCount)
	{
		fprintf(src, "\n\t// Decode the preamble\n");
		fprintf(src, "\tAsnBits SnaccOptionalDefaultBits;\n");
		fprintf(src, "\tbitsDecoded += _b.GetBits(SnaccOptionalDefaultBits, %ld);\n",
			lOptional_Default_ElmtCount);
	}
	
	//******************
	/****RWC; PERFORM PDecContent operations here... ***/
	/* print content local vars */
    //	fprintf (src, "  unsigned int mandatoryElmtsDecoded = 0;\n");
	
	//******************
	/* write extra length vars */
    //	fprintf (src, "{\n");		// RWC; Temporary until I figure out the local var names from combining PDec with PDecContent
    //	for (i = 1; i <= varCount; i++)
    //		fprintf (src, "  %s elmtLen%d = 0; //RWC;default to infinite for now.\n", lenTypeNameG, i);
    //	fprintf (src, "\n");
	
	/* handle empty set */
	//RWC;if ((set->basicType->a.set == NULL) || LIST_EMPTY (set->basicType->a.set))
	if (iElementCount == 0)
	{
		// RWC; Allow for "{" editing...
		/*fprintf (src, "    throw EXCEPT(\"Expected an empty sequence\", DECODE_ERROR);\n");
		fprintf (src, "  }\n");*/
	}
	else
	{
		fprintf(src, "\n\t// Decode each of the elements\n");
		for (ii=0; ii < iElementCount; ii++)
		{
			const char* tabStr = "\t";
			e = pSetElementNamedType[ii];

            if(!e->type->extensionAddition)
            {
			    cxxtri =  e->type->cxxTypeRefInfo;
			    if (e->type->optional || (e->type->defaultVal != NULL))
			    {
				    tabStr = "\t\t";
				    fprintf(src, "\tif (SnaccOptionalDefaultBits.GetBit(%d))\n",
					    iOptional_Default_ElementIndex++);
				    fprintf(src, "\t{\n");
			    }
			    
			    varName = cxxtri->fieldName;
			    
			    /* decode content */
			    if (cxxtri->isPtr)
			    {
				    //fprintf(src, "%sif(%s)\n", tabStr, varName);
                    //fprintf(src, "%s%sdelete %s;\n", tabStr, tabStr, varName);
			    
				    fprintf(src, "%s%s = new %s;\n", tabStr, varName,
						    cxxtri->className);
					     /* END IF subtypes, PER-Visible */
			    }
			    
			    /* decode content */
			    tmpTypeId = GetBuiltinType (e->type);
			    if ((tmpTypeId == BASICTYPE_OCTETCONTAINING) ||
				    (tmpTypeId == BASICTYPE_BITCONTAINING))
			    {
				    PrintPyPERDecodeContaining(e->type, r, src);
			    }
			    else
			    {
				    if (tmpTypeId == BASICTYPE_ANYDEFINEDBY)
				    {
					    elmtLevel++;
				    
					    defByNamedType = e->type->basicType->a.anyDefinedBy->link;
					    PrintPySetTypeByCode(defByNamedType, cxxtri, src);
				    }
				    else if (tmpTypeId == BASICTYPE_ANY)
				    {
					    elmtLevel++;
				    }

				    if (cxxtri->isPtr)
					    fprintf(src, "%s%s->", tabStr, varName);
				    else
					    fprintf(src, "%s%s.", tabStr, varName);
				    fprintf(src, "P%s(_b, bitsDecoded);\n",	r->decodeBaseName);
			    }
			    
			    if (e->type->optional || (e->type->defaultVal != NULL))
				    fprintf (src, "\t}\n\n");
            }				
		} /* for each elmt */
    } /* if not empty set clause */

	fprintf (src, "} // %s::P%s()\n\n", td->cxxTypeDefInfo->className,
		r->decodeBaseName);
		
}		/* END PrintPyDefCode_SetSeqPERDecode(...) */

/*** This routine handles sorting of groups of NameType element(s) based on the
 *   Set and Choice sorting rules.
 */
static void
PrintPyDefCode_PERSort (
    NamedType ***pppElementNamedType, /* OUT, array of sorted NameType(s) */
    int **ppElementTag,      /* OUT, actual tag for sorted. */
    AsnList *pElementList)   /* IN, actual eSNACC defs for NameType(s). */
{
    NamedType **pElementNamedType;
    int *pElementTag;
    NamedType *e;
    NamedType *pnamedTypeTmp;
    Tag *tag;
    TagList *tags;
    int tagTmp;
    int stoleChoiceTags;
    int ii=0, iii;

        /*
         * FIRST, determine encode order by looking at each element tag/type.
         *  (careful with untagged Choice elements, may be nested).
         *  If not tagged in the ASN.1 syntax, then we sort based on the IMPLICIT
         *  tag, even though it may not be encoded for PER.
         * pElementList->count total elements for PER encode sorting.*/
        pElementTag = *ppElementTag = (int *)calloc(pElementList->count, sizeof(int));
        pElementNamedType = *pppElementNamedType = 
            (NamedType **)calloc(pElementList->count, sizeof(NamedType *));
        FOR_EACH_LIST_ELMT (e, pElementList)
        {
            /*RWC;SEE tag-utils.c, line 175 for example of looking at nested
             *RWC;  untagged Choice(s).  For PER, NEED to return lowest tag
             *RWC;  value in nested untagged Choice for sorting.
             *RWC;  The call to GetTags will only return tags with non-tagged
             *RWC;  "Choice" elements if present (flagged by "stoleChoiceTags").*/
            tags = GetTags (e->type, &stoleChoiceTags);

            if (LIST_EMPTY (tags))
            {
                pElementTag[ii] = 0;
                /* RWC; IGNORE; for now */
            }       /* END IF (LIST_EMPTY (tags))*/
            else if (stoleChoiceTags)
            {
                /* FOR untagged Choice, determine lowest possible tag for
                 *  PER sorting order.*/
                pElementTag[ii] = 9999;
                FOR_EACH_LIST_ELMT (tag, tags)
                {
                    if (tag->code < pElementTag[ii])
                       pElementTag[ii] = tag->code;  /* ONLY 1st element for sorting.*/
                }
            }
            else
            {
                tag = (Tag*)FIRST_LIST_ELMT (tags);
                pElementTag[ii] = tag->code;  // ONLY 1st element for sorting.
            }
            pElementNamedType[ii] = e;
            ii++;
        }       // END FOR each element.

        // SECOND, sort this group of elements based on these tags.
        for (ii=0; ii < pElementList->count-1; ii++)
        {
            for (iii=ii+1; iii < pElementList->count; iii++)
            {   // LOCATE smallest tag value
                if (pElementTag[iii] < pElementTag[ii])
                {   // THEN switch them.
                    tagTmp = pElementTag[ii];
                    pnamedTypeTmp = pElementNamedType[ii];
                    pElementTag[ii] = pElementTag[iii];
                    pElementNamedType[ii] = pElementNamedType[iii];
                    pElementTag[iii] = tagTmp;
                    pElementNamedType[iii] = pnamedTypeTmp;
                }
            }   // END for remaining elements (for sorting)
        }       // END FOR each element
}       /* END PrintPyDefCode_PERSort(...) */

void PrintPySimpleDefMeta_1(FILE * hdr, FILE* src, TypeDef* td, int hasNamedElmts, CNamedElmt *n, Module* m)
{

}

void PrintPySimpleDefMeta_2(FILE * hdr, FILE* src, TypeDef* td, int hasNamedElmts, CNamedElmt *n, Module* m, PyRules *r)
{

}

void PrintPyChoiceDefCodeMeta_1(FILE* hdr, FILE* src, TypeDef* td, Type* choice, Module* m, NamedType* e)
{

}

void PrintPySeqDefCodeMeta_1(FILE* hdr, FILE* src, TypeDef* td, Type* seq, Module* m, NamedType* e)
{

}

void PrintPySetDefCodeMeta_1(FILE* hdr, FILE* src, TypeDef* td, Type* set, Module* m, NamedType* e)
{

}
/* EOF gen-code.c (for back-ends/c++-gen) */


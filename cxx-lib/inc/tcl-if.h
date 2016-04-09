#ifndef TCL_IF_H__
#define TCL_IF_H__

#ifdef DEBUG
#include <assert.h>
#endif

#include <tcl.h>

class SNACCDLL_API SnaccTcl
{
    Tcl_Interp *interp;
    Tcl_HashTable modules, types, files;

    Tcl_HashEntry *create();
    const AsnTypeDesc *gettypedesc(const char *cmdname,
                                   const char *type_name);
 public:
    SnaccTcl (Tcl_Interp *);
    ~SnaccTcl();
    
    int create (int argc, char **argv);
    int openfile (int argc, char **argv);
    int finfo (int argc, char **argv);
    int read (int argc, char **argv);
    int write (int argc, char **argv);
    int closefile (int argc, char **argv);
    
    int modulesinfo (int argc, char **argv);
    int typesinfo (int argc, char **argv);
    int typeinfo (int argc, char **argv);
    int info (int argc, char **argv);
    
    int getval (int argc, char **argv);
    int setval (int argc, char **argv);
    int unsetval (int argc, char **argv);
    
    int test (int argc, char **argv);

#ifdef DEBUG
    void ckip (Tcl_Interp *i)
    {
        assert (i == interp);
    }
#endif
};

class SNACCDLL_API ASN1File
{
    const AsnTypeDesc *type;
    AsnType *pdu;

    char *fn;
    int fd;
    off_t filesize;

public:
    ASN1File(const AsnTypeDesc *);
    ASN1File(const AsnTypeDesc *, const char *fn, int fd);
    virtual ~ASN1File();

    bool bad();

    operator AsnType *()
    {
        return pdu;
    }

    int finfo(Tcl_Interp *);

    int read (Tcl_Interp *, const char *fn=NULL);
    int write (Tcl_Interp *, const char *fn=NULL);
};

#endif

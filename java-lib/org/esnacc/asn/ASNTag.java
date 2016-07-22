// Copyright (C) 2016, Aaron Conole

package org.esnacc.asn;

import org.esnacc.utils.SnaccException;
import org.esnacc.utils.Bits;

public class ASNTag implements ASNType
{
    public static final int ANY_CLASS          = -2;
    public static final int NULL_CLASS         = -1;
    public static final int UNIVERSAL_CLASS    = 0;
    public static final int APPLICATION_CLASS  = (1<<6);
    public static final int CONTEXT_CLASS      = (2<<6);
    public static final int PRIVATE_CLASS      = (3<<6);

    public static final int ANY_FORM           = -2;
    public static final int NULL_FORM          = -1;
    public static final int PRIMITIVE_FORM     = 0;
    public static final int CONSTRUCTED_FORM   = (1<<5);

    public enum AsnDataType {
        NO_TYPE,
        BOOLEAN_TYPE,
        INTEGER_TYPE,
        BITSTRING_TYPE,
        OCTETSTRING_TYPE,
        NULL_TYPE,
        OID_TYPE,
        OD_TYPE,
        EXTERNAL_TAG_TYPE,
        REAL_TYPE,
        ENUM_TYPE,
        UTF8STRING_TAG_CODE,
        RELATIVE_OID_TAG_CODE,
        SEQ_TAG_CODE,
        SET_TAG_CODE,
        NUMERICSTRING_TAG_CODE,
        PRINTABLESTRING_TAG_CODE,
        TELETEXSTRING_TAG_CODE,
        VIDEOTEXSTRING_TAG_CODE,
        IA5STRING_TAG_CODE,
        UTCTIME_TAG_CODE,
        GENERALIZEDTIME_TAG_CODE,
        GRAPHICSTRING_TAG_CODE,
        VISIBLESTRING_TAG_CODE,
        GENERALSTRING_TAG_CODE,
        UNIVERSALSTRING_TAG_CODE,
        BMPSTRING_TAG_CODE,
        TT61STRING_TAG_CODE,
        ISO646STRING_TAG_CODE,
    };

    private static int make_tag_id_code(int tagCode)
    {
        if (tagCode < 31) {
            return tagCode << (Bits.sizeof(int.class) - 8);
        } else if (tagCode < 128) {
            return 31 << (Bits.sizeof(int.class) - 8) |
                tagCode << (Bits.sizeof(int.class) - 16);
        } else if (tagCode < 16384) {
            return 31 << (Bits.sizeof(int.class) - 8) |
                ((tagCode & 0x3f80 << 9) |
                 (0x80 << (Bits.sizeof(int.class) - 16)) |
                 (tagCode & 0x007F << (Bits.sizeof(int.class)*24)));
        } 
        return 31 << (Bits.sizeof(int.class) - 8)
            | ((tagCode & 0x1fc000) << 2)
            | (0x0080 << (Bits.sizeof(int.class) - 16))
            | ((tagCode & 0x3f80 << 1) |
               (0x80 << (Bits.sizeof(int.class) - 24)) |
               (tagCode & 0x007F << (Bits.sizeof(int.class)*32)));
    }

    // this is for tag comparison. It doesn't get encoded on the wire.
    public static int MAKE_TAG_ID(int tagClass, int tagForm, int tagCode)
    {
        int SIZE_OF_CLASS_FORM_SHIFT = Bits.sizeof(int.class) - 8;
        return (tagClass << SIZE_OF_CLASS_FORM_SHIFT) |
               (tagForm  << SIZE_OF_CLASS_FORM_SHIFT) |
               (make_tag_id_code(tagCode));
    }

    public int tagClass;
    public int tagForm;
    public int tagType;

    public void encode(Encoder e) throws SnaccException
    {
        e.encodeTag(this);
    }

    public void decode(Decoder d) throws SnaccException
    {
        ASNTag t = d.decodeTag();
        this.tagClass = t.tagClass;
        this.tagForm = t.tagForm;
        this.tagType = t.tagType;
    }

    public int ConstraintCheck(ConstraintsList l) throws SnaccException
    {
        return 0;
    }

}

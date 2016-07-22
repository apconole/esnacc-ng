package org.esnacc.utils;

public class Bits
{
    public static int sizeof(Class dataType)
    {
        if (dataType == null) throw new NullPointerException();

        if (dataType == int.class    || dataType == Integer.class)   return 32;
        if (dataType == short.class  || dataType == Short.class)     return 16;
        if (dataType == byte.class   || dataType == Byte.class)      return 8;
        if (dataType == char.class   || dataType == Character.class) return 16;
        if (dataType == long.class   || dataType == Long.class)      return 64;
        if (dataType == float.class  || dataType == Float.class)     return 32;
        if (dataType == double.class || dataType == Double.class)    return 64;

        return 64; // memory pointer... 
    }
}


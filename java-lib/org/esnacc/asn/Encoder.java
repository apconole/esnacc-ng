// Copyright (C) 2016, Aaron Conole

package org.esnacc.asn;

import org.esnacc.utils.SnaccException;

public abstract class Encoder
{
    public Encoder()
    {
    }

    public abstract void encodeTag(int tagClass, int tagForm, int tagCode)
        throws SnaccException;

    public abstract int tagTypeToCode(int tagType) throws SnaccException;
    
    public void encodeTag(ASNTag tag) throws SnaccException
    {
        encodeTag(tag.tagClass, tag.tagForm, tagTypeToCode(tag.tagType));
    }

}

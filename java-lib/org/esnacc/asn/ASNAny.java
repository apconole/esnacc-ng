// Copyright (C) 2016, Aaron Conole
//


package org.esnacc.asn;

import org.esnacc.utils.SnaccException;
import java.io.Serializable;

public abstract class ASNAny
    implements ASNType, Serializable
{
    public long start;
    public long length;
    public int valueStart;
    public byte data[];

    public ASNAny()
    {
    }

    public void copyFrom(ASNAny any)
    {
        start = any.start;
        length = any.length;
        valueStart = any.valueStart;
        data = any.data;
    }

    public ASNAny(ASNAny any)
    {
        copyFrom(any);
    }

    public Object clone()
    {
        throw new SnaccException("Subclass must implement clone()");
    }

    public void decode(Decoder decoder)
        throws SnaccException
    {
        ASNAny any = decoder.decodeAny();
        copyFrom(any);
    }

    public void encode(Encoder encoder)
        throws SnaccException
    {
        encoder.encodeAny(this);
    }

    public int ConstraintCheck(ConstraintsList l)
    {
        return 0;
    }
}

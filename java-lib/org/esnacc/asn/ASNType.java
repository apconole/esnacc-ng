// Copyright (C) 2016, Aaron Conole

package org.esnacc.asn;

import org.esnacc.utils.SnaccException;

public interface ASNType
{
    public abstract void encode(Encoder e)
        throws SnaccException;

    public abstract void decode(Decoder d)
        throws SnaccException;

    public abstract int ConstraintCheck(ConstraintsList l)
        throws SnaccException;
}

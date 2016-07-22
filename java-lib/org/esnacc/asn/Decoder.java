// Copyright (C) 2016, Aaron Conole

package org.esnacc.asn;

import org.esnacc.utils.SnaccException;

public abstract class Decoder
{
    public abstract ASNTag decodeTag() throws SnaccException;
}

// Copyright (C) 2016, Aaron Conole

package org.esnacc.utils;

public class SnaccException extends Exception
{
    protected Exception except;

    public SnaccException(Exception e)
    {
        except = e;
    }

    public SnaccException(String s)
    {
        super(s);
    }

    public Throwable getCause()
    {
        return except;
    }

    public String getMessage()
    {
        if (except != null) {
            return except.getMessage();
        }

        return super.getMessage();
    }
}

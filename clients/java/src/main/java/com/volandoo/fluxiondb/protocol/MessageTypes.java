package com.volandoo.fluxiondb.protocol;

/**
 * Protocol message type constants for FluxionDB WebSocket communication.
 */
public final class MessageTypes {

    private MessageTypes() {
        // Utility class, prevent instantiation
    }

    public static final String INSERT = "ins";
    public static final String QUERY_RECORDS = "qry";
    public static final String QUERY_COLLECTIONS = "cols";
    public static final String QUERY_DOCUMENT = "qdoc";
    public static final String DELETE_DOCUMENT = "ddoc";
    public static final String DELETE_COLLECTION = "dcol";
    public static final String DELETE_RECORD = "drec";
    public static final String DELETE_MULTIPLE_RECORDS = "dmrec";
    public static final String DELETE_RECORDS_RANGE = "drrng";
    public static final String SET_VALUE = "sval";
    public static final String GET_VALUE = "gval";
    public static final String GET_VALUES = "gvalues";
    public static final String REMOVE_VALUE = "rval";
    public static final String GET_ALL_VALUES = "gvals";
    public static final String GET_ALL_KEYS = "gkeys";
    public static final String MANAGE_API_KEYS = "keys";
    public static final String CONNECTIONS = "conn";

    public static final String READY = "ready";
}

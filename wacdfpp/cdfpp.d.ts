/** CDFpp WebAssembly module type declarations */

type TypedArray =
    | Int8Array
    | Uint8Array
    | Int16Array
    | Uint16Array
    | Int32Array
    | Uint32Array
    | BigInt64Array
    | Float32Array
    | Float64Array;

export interface DataType {
    readonly value: number;
}

export interface Majority {
    readonly value: number;
}

export interface CompressionType {
    readonly value: number;
}

/**
 * A CDF variable returned as a plain JS object.
 * `values` is a zero-copy typed array view into WASM memory — invalidated if the
 * CdfFile is deleted or WASM memory grows.
 * `copy_values` is an owned copy safe to keep indefinitely.
 */
export interface Variable {
    readonly name: string;
    readonly type: number;
    readonly type_name: string;
    readonly shape: number[];
    readonly is_nrv: boolean;
    readonly compression: string;
    readonly values_loaded: boolean;
    readonly attribute_names: string[];
    /**
     * Attribute values keyed by name. Time-typed values (CDF_EPOCH / CDF_EPOCH16 /
     * CDF_TIME_TT2000 — e.g. VALIDMIN/VALIDMAX/FILLVAL on epoch variables) are
     * decoded to an array of ISO-8601 UTC date strings (leap-second corrected, full
     * range — sentinels such as 9999-12-31 render correctly); a plain string is a
     * text attribute; anything else is a numeric typed array. See `attribute_types`
     * for the exact CDF type code.
     */
    readonly attributes: Record<string, string | TypedArray | string[]>;
    /** CDF type code (see DataType) for each attribute in `attributes`. */
    readonly attribute_types: Record<string, number>;
    readonly values: TypedArray | undefined;
    readonly copy_values: TypedArray | undefined;
}

/** A CDF global attribute with one or more entries */
export interface Attribute {
    readonly name: string;
    /**
     * Entry values. Time-typed entries are decoded to an array of ISO-8601 UTC
     * date strings; see `types` for the CDF type code of each entry.
     */
    readonly entries: Array<string | TypedArray | string[]>;
    /** CDF type code (see DataType) for each entry in `entries`. */
    readonly types: number[];
}

/** A loaded CDF file (C++ backed — call delete() when done) */
export interface CdfFile {
    is_valid(): boolean;
    variable_names(): string[];
    attribute_names(): string[];
    get_variable(name: string): Variable;

    /**
     * UTC nanoseconds since 1970 (leap-second corrected) for a time variable
     * (CDF_TIME_TT2000 / CDF_EPOCH / CDF_EPOCH16). The JS analog of
     * datetime64[ns]. Returns undefined for non-time variables.
     */
    time_values_as_ns_since_1970(name: string): BigInt64Array | undefined;

    get_attribute(name: string): Attribute;
    majority(): string;
    compression(): string;

    /** Serialize the CDF file to an owned Uint8Array */
    save(): Uint8Array | undefined;

    /** Free C++ memory — must be called when done */
    delete(): void;
}

export interface CdfModule {
    /** Load a CDF file from a Uint8Array buffer */
    load(data: Uint8Array): CdfFile;

    /** Get the string name of a CDF data type */
    type_name(type: DataType): string;

    /** Get the byte size of a CDF data type */
    type_size(type: DataType): number;

    DataType: {
        CDF_NONE: DataType;
        CDF_INT1: DataType;
        CDF_INT2: DataType;
        CDF_INT4: DataType;
        CDF_INT8: DataType;
        CDF_UINT1: DataType;
        CDF_UINT2: DataType;
        CDF_UINT4: DataType;
        CDF_BYTE: DataType;
        CDF_FLOAT: DataType;
        CDF_REAL4: DataType;
        CDF_DOUBLE: DataType;
        CDF_REAL8: DataType;
        CDF_EPOCH: DataType;
        CDF_EPOCH16: DataType;
        CDF_TIME_TT2000: DataType;
        CDF_CHAR: DataType;
        CDF_UCHAR: DataType;
    };

    Majority: {
        row: Majority;
        column: Majority;
    };

    CompressionType: {
        none: CompressionType;
        rle: CompressionType;
        huffman: CompressionType;
        adaptive_huffman: CompressionType;
        gzip: CompressionType;
    };
}

/** Initialize the CDFpp WASM module */
export default function createCdfModule(): Promise<CdfModule>;

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
    readonly attributes: Record<string, string | TypedArray>;
    readonly values: TypedArray | undefined;
    readonly copy_values: TypedArray | undefined;
}

/** A CDF global attribute with one or more entries */
export interface Attribute {
    readonly name: string;
    readonly entries: Array<string | TypedArray>;
}

/** A loaded CDF file (C++ backed — call delete() when done) */
export interface CdfFile {
    is_valid(): boolean;
    variable_names(): string[];
    attribute_names(): string[];
    get_variable(name: string): Variable;
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

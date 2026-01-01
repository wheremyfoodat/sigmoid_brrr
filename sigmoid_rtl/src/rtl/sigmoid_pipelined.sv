`default_nettype none

import bf16_constants::*;
import lampFPU_pkg::*;

// Pipelined version of the single-cycle sigmoid module
// Pipeline stages:
// Stage 0: Latch input, get absolute value & sign
// Stage 1: Calculate x + offset, pick polynomial coefficients
// Stage 2: Calculate (x + offset) ^ 2 and a1 * (x + offset)
// Stage 3: Calculate a2 * (x + offset) ^ 2 and a1 * (x + offset) = a0
// Stage 4: Calculate final sum and its flipped value (1 - sigmoid(|x|)), choose which one to output based on the sign of the input

// During synthesis we want pipeline stage structs to be packed for better locality/area usage
// During Verilator testing though we want them to not be packed, so that we can easily access pipeline state in C++ code
`ifdef VERILATOR
    `define PREFER_PACKED
`else
    `define PREFER_PACKED packed
`endif

typedef struct `PREFER_PACKED {
    logic valid;
    logic is_negative;
    logic [15:0] x_abs;
} pipeline_stage0_t;

typedef struct `PREFER_PACKED {
    logic valid;
    logic is_negative;
    logic [15:0] x_offset;
    logic [15:0] a0;
    logic [15:0] a1;
    logic [15:0] a2;
} pipeline_stage1_t;

typedef struct `PREFER_PACKED {
    logic valid;
    logic is_negative;
    logic [15:0] x_squared;
    logic [15:0] mul_a1_x;
    logic [15:0] a0;
    logic [15:0] a2;
} pipeline_stage2_t;

typedef struct `PREFER_PACKED {
    logic valid;
    logic is_negative;
    logic [15:0] mul_a2_x2;
    logic [15:0] add_a0_a1;
} pipeline_stage3_t;

typedef struct `PREFER_PACKED {
    logic valid;
    logic [15:0] result;
} pipeline_stage4_t;

// We approximate the sigmoid function as a second degree polynomial
// f(x) = a2 * (x + offset)^2 + a1 * (x + offset) + a0
// Where the polynomial coefficients a0, a1, a2 differ based on the value of x
// And offset is a negative value also based on the value of x (taking values -1, -2, ... -6)
// We have 6 different approximations, one for |x| < 1, another one for |x| < 2, and so on
// For |x| > 6, we consider f(x) = ~0.9999
// We compute the function based on the absolute value of x, and use sigmoid symmetry to calculate it for negative values
// Ie sigmoid(-x) = 1 - sigmoid(x)
module sigmoid_pipelined (
    input wire clk,
    input wire rst,
    input wire valid_in,
    input wire [15:0] data_in,

    output wire valid_out,
    output wire [15:0] data_out

`ifdef VERILATOR
    ,
    output pipeline_stage0_t stage0_out,
    output pipeline_stage1_t stage1_out,
    output pipeline_stage2_t stage2_out,
    output pipeline_stage3_t stage3_out,
    output pipeline_stage4_t stage4_out
`endif
);
    // Current and next pipeline stage data
    // Next fields are set in combinational logic, curr fields in sequential logic
    pipeline_stage0_t stage0_curr, stage0_next;
    pipeline_stage1_t stage1_curr, stage1_next;
    pipeline_stage2_t stage2_curr, stage2_next;
    pipeline_stage3_t stage3_curr, stage3_next;
    pipeline_stage4_t stage4_curr, stage4_next;

    // Absolute value and sign of the input
    wire [15:0] data_in_abs = {1'b0, data_in[14:0]};
    wire is_negative = data_in[15];

    // Coefficients for the 2nd degree polynomial approximation of the sigmoid function
    // Different coefficients are used depending on the value of the input
    logic [15:0] a0;
    logic [15:0] a1;
    logic [15:0] a2;
    logic [15:0] offset; // Offset to subtract from x in polynomial calculation

    // Signals to check whether |x| < less than 1, 2, ..., 6
    localparam [15:0] cmp_values [5:0] = { SIX, FIVE, FOUR, THREE, TWO, ONE }; // Indices are reversed, so ONE is at index 0 and so on
    wire [5:0] less_than;

    // Generate comparators    
    genvar i;
    generate
        for (i = 0; i < 6; i++) begin
            bf16_cmp_lt cmp (
                .op1(stage0_curr.x_abs),
                .op2(cmp_values[i]),
                .cmp_o(less_than[i]),
                .isCmpValid_o()
            );
        end
    endgenerate

    // Pick a set of polynomial coefficients based on the value of |x|
    always_comb begin
        // TODO: Generate for loop
        if (less_than[0]) begin // |x| < 1
            {a2, a1, a0} = {16'hBCE4, 16'h3E85, 16'h3F00}; // -0.027832031, 0.25976563, 0.5
            offset = 16'h0000;
        end

        else if (less_than[1]) begin // |x| < 2
            {a2, a1, a0} = {16'hBD3F, 16'h3E49, 16'h3F3B}; // -0.04663086, 0.19628906, 0.73046875
            offset = MINUS_ONE;
        end

        else if (less_than[2]) begin // |x| < 3
            {a2, a1, a0} = {16'hBCF4, 16'h3DCF, 16'h3F62}; // -0.029785156, 0.10107422, 0.8828125
            offset = MINUS_TWO;
        end

        else if (less_than[3]) begin // |x| < 4
            {a2, a1, a0} = {16'hBC5E, 16'h3D2E, 16'h3F74}; // -0.013549805, 0.04248047, 0.953125
            offset = MINUS_THREE;
        end

        else if (less_than[4]) begin // |x| < 5
            {a2, a1, a0} = {16'hBBB2, 16'h3C87, 16'h3F7B}; // -0.005432129, 0.016479492, 0.98046875
            offset = MINUS_FOUR;
        end

        else if (less_than[5]) begin // |x| < 6
            {a2, a1, a0} = {16'hBB06, 16'h3BCB, 16'h3F7E}; // -0.0020446777, 0.0061950684, 0.9921875
            offset = MINUS_FIVE;
        end

        else begin
            // |x| > 6 approaches one asymptotically
            // TODO: NaNs, Infinities
            {a2, a1, a0} = {16'd0, 16'd0, ONE};
            offset = 16'd0;
        end
    end

    // Stage 0: Fetch input, absolute value, sign
    always_comb begin
        stage0_next.valid = valid_in;
        stage0_next.is_negative = is_negative;
        stage0_next.x_abs = data_in_abs;
    end

    always @(posedge clk) begin
        if (rst) begin 
            stage0_curr.valid <= 'd0;
            stage0_curr.is_negative <= 'd0;
            stage0_curr.x_abs <= 'd0;
        end
        
        else begin
            stage0_curr <= stage0_next;
        end
    end

    // Stage 1: Calculate x + offset and figure out polynomial coefficients

    // x_offset = data_in + offset
    bf16_add_single_cycle add1 (
        .op1(stage0_curr.x_abs),
        .op2(offset),
        .result(stage1_next.x_offset),
        .isResultValid(),
        .isReady()
    );

    always_comb begin
        stage1_next.valid = stage0_curr.valid;
        stage1_next.is_negative = stage0_curr.is_negative;
        stage1_next.a0 = a0;
        stage1_next.a1 = a1;
        stage1_next.a2 = a2;
    end

    always @(posedge clk) begin
        if (rst) begin 
            stage1_curr.valid <= 'd0;
            stage1_curr.is_negative <= 'd0;
            stage1_curr.x_offset <= 'd0;
            stage1_curr.a0 <= 'd0;
            stage1_curr.a1 <= 'd0;
            stage1_curr.a2 <= 'd0;
        end
        
        else begin
            stage1_curr <= stage1_next;
        end
    end

    // Stage 2: Calculate (x + offset) ^ 2 and a1 * (x + offset)

    // x_squared = (x + offset)^2
    bf16_mul_single_cycle mul1 (
        .op1(stage1_curr.x_offset),
        .op2(stage1_curr.x_offset),
        .result(stage2_next.x_squared),
        .isResultValid(),
        .isReady()
    );

    // mul2_output = a1 * (x + offset)
    bf16_mul_single_cycle mul2 (
        .op1(stage1_curr.a1),
        .op2(stage1_curr.x_offset),
        .result(stage2_next.mul_a1_x),
        .isResultValid(),
        .isReady()
    );

    // Pass through misc state from stage 0 to stage 1
    always_comb begin
        stage2_next.valid = stage1_curr.valid;
        stage2_next.is_negative = stage1_curr.is_negative;
        stage2_next.a0 = stage1_curr.a0; // a1 is not needed after stage 1
        stage2_next.a2 = stage1_curr.a2;
    end

    always @(posedge clk) begin
        if (rst) begin 
            stage2_curr.valid <= 'd0;
            stage2_curr.is_negative <= 'd0;
            stage2_curr.x_squared <= 'd0;
            stage2_curr.mul_a1_x <= 'd0;
            stage2_curr.a0 <= 'd0;
            stage2_curr.a2 <= 'd0;
        end
        
        else begin
            stage2_curr <= stage2_next;
        end
    end

    // Stage 3: Calculate a2 * (x + offset) ^ 2 and a1 * (x + offset) = a0

    // mul3_output = a2 * (x + offset)^2
    bf16_mul_single_cycle mul3 (
        .op1(stage2_curr.a2),
        .op2(stage2_curr.x_squared),
        .result(stage3_next.mul_a2_x2),
        .isResultValid(),
        .isReady()
    );

    // add2_output = a0 + a1 * (x + offset)
    bf16_add_single_cycle add2 (
        .op1(stage2_curr.a0),
        .op2(stage2_curr.mul_a1_x),
        .result(stage3_next.add_a0_a1),
        .isResultValid(),
        .isReady()
    );

    // Pass through state from stage 1 to stage 2
    always_comb begin
        stage3_next.valid = stage2_curr.valid;
        stage3_next.is_negative = stage2_curr.is_negative;
    end

    always @(posedge clk) begin
        if (rst) begin 
            stage3_curr.valid <= 'd0;
            stage3_curr.is_negative <= 'd0;
            stage3_curr.mul_a2_x2 <= 'd0;
            stage3_curr.add_a0_a1 <= 'd0;
        end
        
        else begin
            stage3_curr <= stage3_next;
        end
    end

    // Stage 4: Calculate final sum and output
    // We calculate sigmoid by taking the absolute value of the input and passing it to the polynomial
    // Then for negative values, we can do sigmoid(x) = 1 - sigmoid(|x|)
    logic [15:0] polynomial_output;
    logic [15:0] one_minus_polynomial_output;

    // Computes final result
    bf16_add_single_cycle add3 (
        .op1(stage3_curr.add_a0_a1),
        .op2(stage3_curr.mul_a2_x2),
        .result(polynomial_output),
        .isResultValid(),
        .isReady()
    );

    // Compute inverse of polynomial
    bf16_sub_single_cycle flip_poly (
        .op1(ONE),
        .op2(polynomial_output),
        .result(one_minus_polynomial_output),
        .isResultValid(),
        .isReady()
    );

    // Pass through valid flag from stage 2 to stage 3, pick final result based on sign of input
    always_comb begin
        stage4_next.valid = stage3_curr.valid;
        stage4_next.result = (stage3_curr.is_negative == 0) ? polynomial_output : one_minus_polynomial_output;
    end

    always @(posedge clk) begin
        if (rst) begin 
            stage4_curr.valid <= 'd0;
            stage4_curr.result <= 'd0;
        end
        
        else begin
            stage4_curr <= stage4_next;
        end
    end

    // Final pipeline output
    assign valid_out = stage4_curr.valid;
    assign data_out = stage4_curr.result;

    // Expose pipeline stage outputs so that we can access them from the Verilator testbench
`ifdef VERILATOR
    assign stage0_out = stage0_curr;
    assign stage1_out = stage1_curr;
    assign stage2_out = stage2_curr;
    assign stage3_out = stage3_curr;
    assign stage4_out = stage4_curr;
`endif
endmodule

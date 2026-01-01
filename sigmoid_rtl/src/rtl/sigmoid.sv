`default_nettype none

import bf16_constants::*;

// We approximate the sigmoid function as a second degree polynomial
// f(x) = a2 * (x + offset)^2 + a1 * (x + offset) + a0
// Where the polynomial coefficients a0, a1, a2 differ based on the value of x
// And offset is a negative value also based on the value of x (taking values -1, -2, ... -6)
// We have 6 different approximations, one for |x| < 1, another one for |x| < 2, and so on
// For |x| > 6, we consider f(x) = ~0.9999
// We compute the function based on the absolute value of x, and use sigmoid symmetry to calculate it for negative values
// Ie sigmoid(-x) = 1 - sigmoid(x)
module sigmoid (
    input wire clk,
    input wire rst,
    input wire valid_in,
    input wire [15:0] data_in,

    output wire valid_out,
    output wire [15:0] data_out
);
    // Absolute value and sign of the input
    wire [15:0] data_in_abs = {1'b0, data_in[14:0]};
    wire is_negative = data_in[15];

    // Coefficients for the 2nd degree polynomial approximation of the sigmoid function
    // Different coefficients are used depending on 
    logic [15:0] a0;
    logic [15:0] a1;
    logic [15:0] a2;
    logic [15:0] offset; // Offset to subtract from x in polynomial calculation
    
    // We calculate sigmoid by taking the absolute value of the input and passing it to the polynomial
    // Then for negative values, we can do sigmoid(x) = 1 - sigmoid(|x|)
    logic [15:0] polynomial_output;
    logic [15:0] one_minus_polynomial_output;
    assign data_out = !is_negative ? polynomial_output : one_minus_polynomial_output;

    // Signals to check whether |x| < less than 1, 2, ..., 6
    localparam [15:0] cmp_values [5:0] = { SIX, FIVE, FOUR, THREE, TWO, ONE }; // Indices are reversed, so ONE is at index 0 and so on
    wire [5:0] less_than;

    // Generate comparators    
    genvar i;
    generate
        for (i = 0; i < 6; i++) begin
            bf16_cmp_lt cmp (
                .op1(data_in_abs),
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

    polynomial_2nd_degree poly (
        .clk(clk),
        .rst(rst),
        .valid_in(valid_in),
        .valid_out(valid_out),
        .data_in(data_in_abs),
        .data_out(polynomial_output),

        .a0(a0),
        .a1(a1),
        .a2(a2),
        .offset(offset)
    );

    bf16_sub_single_cycle flip_poly (
        .op1(ONE),
        .op2(polynomial_output),
        .result(one_minus_polynomial_output),
        .isResultValid(),
        .isReady()
    );
endmodule

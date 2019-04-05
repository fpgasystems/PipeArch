module float_scalar_vector_mult
#(
	parameter VALUES_PER_LINE = 16
)
(
    input logic clk,
    input logic reset,
    input logic [31:0] scalar,
    input logic [32*VALUES_PER_LINE-1:0] vector,
    input logic trigger,
    output logic [32*VALUES_PER_LINE-1:0] result,
    output logic result_valid
);

logic [VALUES_PER_LINE-1:0] valid;
genvar index;
generate
    for(index = 0; index < VALUES_PER_LINE; index = index + 1)
    begin: gen_mult
        float_mult
        mult (
            .clk(clk),
            .reset(reset),
            .in1(scalar),
            .in2(vector[index*32+31 -: 32]),
            .in_valid(trigger),
            .q(result[index*32+31 -: 32]),
            .q_valid(valid[index]));
    end
endgenerate
assign result_valid = valid[0];

endmodule // float_vector_mult
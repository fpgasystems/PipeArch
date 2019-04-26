`include "pipearch_common.vh"

module float_l1reg
(
    input logic clk,
    input logic reset,
    input logic [31:0] in_model,
    input logic [31:0] in_step,
    input logic [31:0] in_lambda,
    input logic in_valid,
    output logic [31:0] out_step,
    output logic out_valid
);

    logic [31:0] in_minuslambda;
    assign in_minuslambda = {!in_lambda[31], in_lambda[30:0]};

    logic [31:0] model_minus_step;
    logic model_minus_step_valid;
    float_subtract
    model_minus_step_subtract (
        .clk(clk),
        .reset(reset),
        .in1(in_model),
        .in2(in_step),
        .in_valid(in_valid),
        .q(model_minus_step),
        .q_valid(model_minus_step_valid));

    logic [31:0] step_plus_lambda;
    logic step_plus_lambda_valid;
    float_subtract
    step_plus_lambda_subtract (
        .clk(clk),
        .reset(reset),
        .in1(in_step),
        .in2(in_minuslambda),
        .in_valid(in_valid),
        .q(step_plus_lambda),
        .q_valid(step_plus_lambda_valid));

    logic [31:0] step_minus_lambda;
    logic step_minus_lambda_valid;
    float_subtract
    step_minus_lambda_subtract (
        .clk(clk),
        .reset(reset),
        .in1(in_step),
        .in2(in_lambda),
        .in_valid(in_valid),
        .q(step_minus_lambda),
        .q_valid(step_minus_lambda_valid));

    logic gt_result;
    logic gt_result_valid;
    float_greaterthan
    model_minus_step_gt (
        .clk(clk),
        .reset(reset),
        .in1(model_minus_step),
        .in2(in_lambda),
        .in_valid(model_minus_step_valid),
        .q(gt_result),
        .q_valid(gt_result_valid));

    logic lt_result;
    logic lt_result_valid;
    float_lessthan
    model_minus_step_lt (
        .clk(clk),
        .reset(reset),
        .in1(model_minus_step),
        .in2(in_minuslambda),
        .in_valid(model_minus_step_valid),
        .q(lt_result),
        .q_valid(lt_result_valid));

    always_ff @(posedge clk)
    begin
        if (gt_result == 1'b1) begin
            out_step <= step_plus_lambda;
        end
        else if (lt_result == 1'b1) begin
            out_step <= step_minus_lambda;
        end
        else begin
            out_step <= in_model;
        end
        out_valid <= gt_result_valid;
    end

endmodule // float_l1reg
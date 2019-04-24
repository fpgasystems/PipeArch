`include "pipearch_common.vh"

module pipearch_loadreg
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [3],
    output logic [31:0] outregs[5],
    
    fifobram_interface.read REGION_read
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_RECEIVE
    } t_loadregstate;
    t_loadregstate loadreg_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] offset_by_index;
    logic [3:0] position_by_index;
    logic [2:0] which_register;
    logic [15:0] line_offset;

    always_ff @(posedge clk)
    begin
        REGION_read.re <= 1'b0;
        op_done <= 1'b0;

        case (loadreg_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    offset_by_index <= regs[0][15:0];
                    position_by_index <= regs[0][3:0];
                    which_register <= regs[1][2:0];
                    line_offset <= regs[2][15:0];
                    loadreg_state <= STATE_READ;
                end
            end

            STATE_READ:
            begin
                REGION_read.re <= 1'b1;
                REGION_read.raddr <= line_offset + offset_by_index[15:4];
                loadreg_state <= STATE_RECEIVE;
            end

            STATE_RECEIVE:
            begin
                if (REGION_read.rvalid)
                begin
                    if (which_register == 3) begin
                        outregs[3] <= REGION_read.rdata[position_by_index*32+31 -: 32];
                    end
                    else if (which_register == 4) begin
                        outregs[4] <= REGION_read.rdata[position_by_index*32+31 -: 32];
                    end
                    op_done <= 1'b1;
                    loadreg_state <= STATE_IDLE;
                end
            end
        endcase

        if (reset)
        begin
            loadreg_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_loadreg
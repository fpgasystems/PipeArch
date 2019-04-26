`include "pipearch_common.vh"

module write_region
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,
    input logic [15:0] iterations,

    internal_interface.commonwrite_source into_write,
    fifobram_interface.read props_access,
    fifobram_interface.write region_access
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_WRITE,
        STATE_FETCH_PROPS,
        STATE_RECEIVE_PROPS,
        STATE_WRITE_WITH_PROPS
    } t_readstate;
    t_readstate receive_state;

    assign into_write.almostfull = region_access.almostfull || receive_state == STATE_FETCH_PROPS || receive_state == STATE_RECEIVE_PROPS;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic[15:0] num_iterations;
    access_properties memory_store;
    logic [LOG2_ACCESS_SIZE-1:0] accessprops_offset;
    logic [LOG2_ACCESS_SIZE-1:0] accessprops_length;
    logic [17:0] accessprops_raddr;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [LOG2_ACCESS_SIZE-1:0] num_received_lines;
    logic [15:0] num_performed_iterations;
    logic [3:0] accessprops_position;

    always_ff @(posedge clk)
    begin
        props_access.re <= 1'b0;
        region_access.we <= 1'b0;
        region_access.wdata <= into_write.wdata;
        region_access.wfifobram <= {memory_store.write_fifo, memory_store.write_bram};

        case (receive_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    memory_store.write_bram <= configreg[30];
                    memory_store.write_fifo <= configreg[31];
                    memory_store.offset <= configreg[13:0];
                    memory_store.length <= configreg[29:16];
                    num_iterations <= iterations;
                    memory_store.use_local_props <= configreg[14];
                    memory_store.keep_count_along_iterations <= configreg[15];
                    accessprops_raddr <= {configreg[13:0], 4'b0000};
                    // *************************************************************************
                    num_received_lines <= 0;
                    num_performed_iterations <= 0;
                    if (configreg[14])
                    begin
                        receive_state <= STATE_FETCH_PROPS;
                    end
                    else if (configreg[29:16] == 16'b0 || configreg[31:30] == 2'b0)
                    begin
                        receive_state <= STATE_IDLE;
                    end
                    else
                    begin
                        receive_state <= STATE_WRITE;
                    end
                end
            end

            STATE_FETCH_PROPS:
            begin
                props_access.re <= 1'b1;
                props_access.rfifobram <= 2'b01;
                props_access.raddr <= accessprops_raddr >> 4;
                accessprops_position <= accessprops_raddr[3:0];
                receive_state <= STATE_RECEIVE_PROPS;
            end

            STATE_RECEIVE_PROPS:
            begin
                if (props_access.rvalid)
                begin
                    accessprops_offset <= props_access.rdata[accessprops_position*32+13 -: 14];
                    accessprops_length <= props_access.rdata[accessprops_position*32+29 -: 14];
                    receive_state <= STATE_WRITE_WITH_PROPS;
                end
            end

            STATE_WRITE_WITH_PROPS:
            begin
                if (into_write.we && num_received_lines < accessprops_length)
                begin
                    region_access.we <= 1'b1;
                    region_access.waddr <= accessprops_length + num_received_lines;
                    num_received_lines <= num_received_lines + 1;
                    if (num_received_lines == accessprops_length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            receive_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_received_lines <= 0;
                            if (memory_store.keep_count_along_iterations)
                            begin
                                accessprops_raddr <= accessprops_raddr + memory_store.length;
                            end
                            receive_state <= STATE_FETCH_PROPS;
                        end
                    end
                end
            end

            STATE_WRITE:
            begin
                if (into_write.we && num_received_lines < memory_store.length)
                begin
                    region_access.we <= 1'b1;
                    region_access.waddr <= memory_store.offset + num_received_lines;
                    num_received_lines <= num_received_lines + 1;
                    if (num_received_lines == memory_store.length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            receive_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_received_lines <= 0;
                            if (memory_store.keep_count_along_iterations)
                            begin
                                memory_store.offset <= memory_store.offset + memory_store.length;
                            end
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            receive_state <= STATE_IDLE;
            memory_store <= 0;
        end
    end
endmodule // write_region
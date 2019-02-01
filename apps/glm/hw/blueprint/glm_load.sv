`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module glm_load
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],
    input t_ccip_clAddr in_addr,

    // CCI-P request/response
    input  c0TxAlmFull,
    input  t_if_ccip_c0_Rx cp2af_sRx_c0,
    output t_if_ccip_c0_Tx af2cp_sTx_c0,

    fifobram_interface.fifo_write FIFO_input,
    fifobram_interface.fifo_write FIFO_samplesforward,
    fifobram_interface.bram_write MEM_model,
    fifobram_interface.bram_write MEM_labels
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;
    t_readstate receive_state;

    internal_interface #(.WIDTH(512)) from_load();
    // *************************************************************************
    //
    //   Load Channels
    //
    // *************************************************************************
    internal_interface #(.WIDTH(512)) from_load_to_FIFO_input();
    write_fifo
    write_FIFO_input_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[5]),
        .into_write(from_load_to_FIFO_input.commonwrite_source),
        .fifo_access(FIFO_input)
    );

    internal_interface #(.WIDTH(512)) from_load_to_FIFO_samplesforward();
    write_fifo
    write_FIFO_samplesforward_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[6]),
        .into_write(from_load_to_FIFO_samplesforward.commonwrite_source),
        .fifo_access(FIFO_samplesforward)
    );

    internal_interface #(.WIDTH(512)) from_load_to_MEM_model();
    write_bram
    write_MEM_model_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[7]),
        .into_write(from_load_to_MEM_model.commonwrite_source),
        .memory_access(MEM_model)
    );

    internal_interface #(.WIDTH(512)) from_load_to_MEM_labels();
    write_bram
    write_MEM_labels_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[8]),
        .into_write(from_load_to_MEM_labels.commonwrite_source),
        .memory_access(MEM_labels)
    );

    always_comb
    begin
        from_load_to_FIFO_input.we = from_load.we;
        from_load_to_FIFO_input.wdata = from_load.wdata;
        from_load_to_FIFO_samplesforward.we = from_load.we;
        from_load_to_FIFO_samplesforward.wdata = from_load.wdata;
        from_load_to_MEM_model.we = from_load.we;
        from_load_to_MEM_model.wdata = from_load.wdata;
        from_load_to_MEM_labels.we = from_load.we;
        from_load_to_MEM_labels.wdata = from_load.wdata;
    end
    assign from_load.almostfull = from_load_to_FIFO_input.almostfull | from_load_to_FIFO_samplesforward.almostfull | from_load_to_MEM_model.almostfull | from_load_to_MEM_labels.almostfull;

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) prefetch_fifo_access();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .reset,
        .access(prefetch_fifo_access.fifo_source)
    );
    assign prefetch_fifo_access.we = cci_c0Rx_isReadRsp(cp2af_sRx_c0) && (receive_state == STATE_READ);
    assign prefetch_fifo_access.wdata = cp2af_sRx_c0.data;
    always_ff @(posedge clk)
    begin
        prefetch_fifo_access.re <= !(prefetch_fifo_access.empty) && !(from_load.almostfull);
    end

    t_ccip_clAddr DRAM_load_offset;
    logic [31:0] DRAM_load_length;

    // Counters
    logic [31:0] num_requested_lines;
    logic [31:0] num_received_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    always_ff @(posedge clk)
    begin
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_access.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);

        af2cp_sTx_c0.hdr <= t_cci_c0_ReqMemHdr'(0);
        af2cp_sTx_c0.valid <= 1'b0;

        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;
            num_requested_lines <= 32'b0;
            num_received_lines <= 32'b0;
            from_load.we <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin
            
            // =================================
            //
            //   Request State Machine
            //
            // =================================
            case (request_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        DRAM_load_offset <= in_addr + regs[3];
                        DRAM_load_length <= regs[4];
                        num_requested_lines <= 32'b0;
                        if (regs[4] == 0)
                        begin
                            request_state <= STATE_DONE;
                        end
                        else
                        begin
                            request_state <= STATE_READ;
                        end
                    end
                end

                STATE_READ:
                begin
                    if (num_requested_lines < DRAM_load_length && !c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                    begin
                        af2cp_sTx_c0.valid <= 1'b1;
                        af2cp_sTx_c0.hdr.address <= DRAM_load_offset + num_requested_lines;

                        num_requested_lines <= num_requested_lines + 1;
                        if (num_requested_lines == DRAM_load_length-1)
                        begin
                            request_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    request_state <= STATE_IDLE;
                end
            endcase


            from_load.we <= 1'b0;
            op_done <= 1'b0;
            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            case (receive_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        num_received_lines <= 32'b0;
                        if (regs[4] == 0)
                        begin
                            receive_state <= STATE_DONE;
                        end
                        else
                        begin
                            receive_state <= STATE_READ;
                        end
                    end
                end

                STATE_READ:
                begin
                    if (prefetch_fifo_access.rvalid)
                    begin
                        from_load.we <= 1'b1;
                        from_load.wdata <= prefetch_fifo_access.rdata;
                        num_received_lines <= num_received_lines + 1;
                        if (num_received_lines == DRAM_load_length-1)
                        begin
                            receive_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    receive_state <= STATE_IDLE;
                end
            endcase
        end
    end

endmodule // glm_load
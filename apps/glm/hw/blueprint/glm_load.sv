`include "cci_mpf_if.vh"
`include "pipearch_common.vh"
`include "glm_common.vh"

module glm_load
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [5+NUM_LOAD_CHANNELS],
    input t_ccip_clAddr in_addr,

    // CCI-P request/response
    input  c0TxAlmFull,
    input  t_if_ccip_c0_Rx cp2af_sRx_c0,
    output t_if_ccip_c0_Tx af2cp_sTx_c0,

    fifobram_interface.fifo_write FIFO_input,
    fifobram_interface.fifo_write FIFO_samplesforward,
    fifobram_interface.bram_write MEM_model,
    fifobram_interface.bram_write MEM_labels,
    fifobram_interface.bram_readwrite MEM_accessprops
);

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

    internal_interface #(.WIDTH(512)) from_load_to_MEM_accessprops();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) load_MEM_accessprops_interface();
    write_bram
    write_MEM_accessprops_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[9]),
        .into_write(from_load_to_MEM_accessprops.commonwrite_source),
        .memory_access(load_MEM_accessprops_interface.bram_write)
    );
    assign MEM_accessprops.we = load_MEM_accessprops_interface.we;
    assign MEM_accessprops.waddr = load_MEM_accessprops_interface.waddr;
    assign MEM_accessprops.wdata = load_MEM_accessprops_interface.wdata;

    always_ff @(posedge clk)
    begin
        from_load_to_FIFO_input.we <= from_load.we;
        from_load_to_FIFO_input.wdata <= from_load.wdata;
        from_load_to_FIFO_samplesforward.we <= from_load.we;
        from_load_to_FIFO_samplesforward.wdata <= from_load.wdata;
        from_load_to_MEM_model.we <= from_load.we;
        from_load_to_MEM_model.wdata <= from_load.wdata;
        from_load_to_MEM_labels.we <= from_load.we;
        from_load_to_MEM_labels.wdata <= from_load.wdata;
        from_load_to_MEM_accessprops.we <= from_load.we;
        from_load_to_MEM_accessprops.wdata <= from_load.wdata;
        from_load.almostfull <= from_load_to_FIFO_input.almostfull | from_load_to_FIFO_samplesforward.almostfull | from_load_to_MEM_model.almostfull | from_load_to_MEM_labels.almostfull;
    end 

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_PREPROCESS,
        STATE_FETCH_ACCESSPROPS,
        STATE_RECEIVE_ACCESSPROPS,
        STATE_ADD_ACCESSPROPS,
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;
    t_readstate receive_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic[31:0] offset_by_index[3];
    logic [$bits(t_ccip_clAddr)-32-1:0] upper_part_address;
    logic [31:0] running_DRAM_load_offset;
    logic [31:0] DRAM_load_length;
    logic enable_multiline;
    logic use_accessprops;
    logic [LOG2_MEMORY_SIZE-1:0] accessprops_raddr;
    logic gen_syn_data;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [2:0] accessprops_position;
    logic [31:0] accessprops_DRAM_offset;
    logic [31:0] accessprops_DRAM_length;
    logic [1:0] offset_accumulate;
    logic [31:0] num_requested_lines;
    logic [31:0] num_requested_lines_plus2;
    logic [31:0] num_requested_lines_plus4;
    logic [31:0] num_received_lines;
    logic [31:0] num_forward_request_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    // *************************************************************************
    //
    //   Receive FIFO
    //
    // *************************************************************************
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
    
    always_ff @(posedge clk)
    begin
        prefetch_fifo_access.we <= 1'b0;
        if (op_start)
        begin
            num_received_lines <= 32'b0;
        end
        if (cci_c0Rx_isReadRsp(cp2af_sRx_c0) && receive_state == STATE_READ)
        begin
            num_received_lines <= num_received_lines + 1;
            prefetch_fifo_access.we <= 1'b1;
            if (gen_syn_data)
            begin
                for (int i=0; i < 16; i=i+1)
                begin
                    prefetch_fifo_access.wdata[ (i*32)+31 -: 32 ] <= 32'(i);
                end
            end
            else
            begin
                prefetch_fifo_access.wdata <= cp2af_sRx_c0.data;
            end
        end
        prefetch_fifo_access.re <= !(prefetch_fifo_access.empty) && !(from_load.almostfull);
    end

    always_ff @(posedge clk)
    begin
        // =================================
        //
        //   Calculate Allowed Lines
        //
        // =================================
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_access.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);

        af2cp_sTx_c0.hdr <= t_cci_c0_ReqMemHdr'(0);
        af2cp_sTx_c0.valid <= 1'b0;
        from_load.we <= 1'b0;
        MEM_accessprops.re <= 1'b0;
        op_done <= 1'b0;

        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;
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
                        // *************************************************************************
                        offset_by_index[0] <= regs[0];
                        offset_by_index[1] <= regs[1];
                        offset_by_index[2] <= regs[2];
                        upper_part_address <= in_addr[$bits(t_ccip_clAddr)-1:32];
                        running_DRAM_load_offset <= (regs[3][31] == 1'b1) ? in_addr[31:0] : in_addr[31:0] + regs[3][29:0];
                        DRAM_load_length <= regs[4][30:0];
                        enable_multiline <= regs[4][31];
                        use_accessprops <= regs[3][31];
                        accessprops_raddr <= regs[3][29:0];
                        gen_syn_data <= regs[3][30];
                        // *************************************************************************
                        accessprops_position <= 0;
                        offset_accumulate <= 2'b0;
                        num_requested_lines <= 32'b0;
                        num_requested_lines_plus4 <= 32'd4;
                        num_requested_lines_plus2 <= 32'd2;
                        if (regs[4][30:0] == 0 && regs[3][31] == 0)
                        begin
                            request_state <= STATE_DONE;
                        end
                        else
                        begin
                            request_state <= STATE_PREPROCESS;
                        end
                    end
                end

                STATE_PREPROCESS:
                begin
                    if (use_accessprops)
                    begin
                        accessprops_raddr <= accessprops_raddr + offset_by_index[offset_accumulate];
                        if (offset_accumulate == 2)
                        begin
                            request_state <= STATE_FETCH_ACCESSPROPS;
                        end
                    end
                    else
                    begin
                        running_DRAM_load_offset <= running_DRAM_load_offset + offset_by_index[offset_accumulate];
                        if (offset_accumulate == 2)
                        begin
                            request_state <= STATE_READ;
                        end
                    end
                    offset_accumulate <= offset_accumulate + 1;
                end

                STATE_FETCH_ACCESSPROPS:
                begin
                    MEM_accessprops.re <= 1'b1;
                    MEM_accessprops.raddr <= accessprops_raddr >> 3;
                    accessprops_position <= accessprops_raddr[2:0];
                    request_state <= STATE_RECEIVE_ACCESSPROPS;
                end

                STATE_RECEIVE_ACCESSPROPS:
                begin
                    if (MEM_accessprops.rvalid)
                    begin
                        accessprops_DRAM_offset <= MEM_accessprops.rdata[accessprops_position*64+31 -: 32];
                        accessprops_DRAM_length <= MEM_accessprops.rdata[accessprops_position*64+63 -: 32];
                        request_state <= STATE_ADD_ACCESSPROPS;
                    end
                end

                STATE_ADD_ACCESSPROPS:
                begin
                    running_DRAM_load_offset <= running_DRAM_load_offset + accessprops_DRAM_offset;
                    DRAM_load_length <= accessprops_DRAM_length;
                    request_state <= STATE_READ;
                end

                STATE_READ:
                begin
                    if (!c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                    begin
                        if (enable_multiline && 
                            (num_requested_lines_plus4 < DRAM_load_length) && 
                            (running_DRAM_load_offset[1:0] == 2'b0))
                        begin
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'({upper_part_address, running_DRAM_load_offset});
                            af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                            af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_4;
                            num_requested_lines <= num_requested_lines_plus4;
                            num_requested_lines_plus2 <= num_requested_lines_plus4 + 2;
                            num_requested_lines_plus4 <= num_requested_lines_plus4 + 4;
                            running_DRAM_load_offset <= running_DRAM_load_offset + 4;
                        end
                        else if (enable_multiline && 
                            (num_requested_lines_plus2 < DRAM_load_length) &&
                            (running_DRAM_load_offset[0] == 1'b0))
                        begin
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'({upper_part_address, running_DRAM_load_offset});
                            af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                            af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_2;
                            num_requested_lines <= num_requested_lines_plus2;
                            num_requested_lines_plus2 <= num_requested_lines_plus2 + 2;
                            num_requested_lines_plus4 <= num_requested_lines_plus2 + 4;
                            running_DRAM_load_offset <= running_DRAM_load_offset + 2;
                        end
                        else if (num_requested_lines < DRAM_load_length)
                        begin
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'({upper_part_address, running_DRAM_load_offset});
                            af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                            af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_1;
                            num_requested_lines <= num_requested_lines + 1;
                            num_requested_lines_plus2 <= num_requested_lines + 3;
                            num_requested_lines_plus4 <= num_requested_lines + 5;
                            running_DRAM_load_offset <= running_DRAM_load_offset + 1;
                        end
                    end

                    if (num_requested_lines == DRAM_load_length)
                    begin
                        request_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    request_state <= STATE_IDLE;
                end
            endcase

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
                        num_forward_request_lines <= 32'b0;
                        if (regs[4][30:0] == 0 && regs[3][31] == 0)
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
                        num_forward_request_lines <= num_forward_request_lines + 1;
                        if (num_forward_request_lines == DRAM_load_length-1)
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
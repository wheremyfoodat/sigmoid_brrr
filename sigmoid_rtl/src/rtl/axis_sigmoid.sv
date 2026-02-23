`default_nettype none
`timescale 1ns / 1ps

module axis_sigmoid (
    input  wire        aclk,
    input  wire        aresetn,
    // S_AXIS
    input  wire [15:0] s_axis_tdata,
    input  wire        s_axis_tlast,
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,
    // M_AXIS
    output wire [15:0] m_axis_tdata,
    output wire        m_axis_tlast,
    output wire        m_axis_tvalid,
    input  wire        m_axis_tready
);

  // -------------------------------------------------------------------------
  // Parameters
  // -------------------------------------------------------------------------
  localparam integer PIPELINE_LATENCY = 6;
  localparam integer FIFO_DEPTH = 32;
  localparam integer PROG_FULL_THRESH = FIFO_DEPTH - PIPELINE_LATENCY;
  localparam integer FIFO_WIDTH = 16 + 1;  // 16 bits Data + 1 bit TLast


  logic [          15:0] core_data_out;
  logic                  core_valid_out;

  wire                   fifo_prog_full;
  wire                   input_accepted;

  // FIFO Signals
  wire                   fifo_empty;
  wire                   fifo_rd_en;
  wire  [FIFO_WIDTH-1:0] fifo_dout;
  wire  [FIFO_WIDTH-1:0] fifo_din;

  // Input Logic & Backpressure
  assign s_axis_tready  = ~fifo_prog_full && aresetn;
  assign input_accepted = s_axis_tvalid && s_axis_tready;

  // TLAST (Sideband Delay)
  reg [PIPELINE_LATENCY-1:0] tlast_pipe;

  always_ff @(posedge aclk) begin
    if (!aresetn) begin
      tlast_pipe <= '0;
    end
    begin
      tlast_pipe <= {tlast_pipe[PIPELINE_LATENCY-2:0], s_axis_tlast};
    end
  end

  wire delayed_tlast = tlast_pipe[PIPELINE_LATENCY-1];


  sigmoid_pipelined inst_sigmoid (
      .clk     (aclk),
      .rst     (~aresetn),
      .valid_in(input_accepted),
      .data_in (s_axis_tdata),

      .valid_out(core_valid_out),
      .data_out (core_data_out)
  );


  // Pack Data and Tlast together: [TLAST | DATA]
  assign fifo_din = {delayed_tlast, core_data_out};

  xpm_fifo_sync #(
      .FIFO_MEMORY_TYPE("auto"),
      .FIFO_WRITE_DEPTH(FIFO_DEPTH),
      .WRITE_DATA_WIDTH(FIFO_WIDTH),
      .READ_DATA_WIDTH(FIFO_WIDTH),
      .READ_MODE("fwft"),
      .PROG_FULL_THRESH(PROG_FULL_THRESH),
      .USE_ADV_FEATURES("0200")  // Enable prog_full (Bit 1)
  ) xpm_fifo_inst (
      .wr_clk   (aclk),
      .rst      (~aresetn),        // XPM Sync FIFO reset is Active High
      // -- Write Interface --
      .wr_en    (core_valid_out),
      .din      (fifo_din),
      .full     (),
      .prog_full(fifo_prog_full),
      // -- Read Interface --
      .rd_en    (fifo_rd_en),
      .dout     (fifo_dout),
      .empty    (fifo_empty),

      // Unused
      .overflow     (),
      .wr_rst_busy  (),
      .rd_rst_busy  (),
      .prog_empty   (),
      .underflow    (),
      .data_valid   (),
      .sleep        (1'b0),
      .injectsbiterr(1'b0),
      .injectdbiterr(1'b0),
      .sbiterr      (),
      .dbiterr      ()
  );


  // Data un-packing
  assign m_axis_tvalid = ~fifo_empty;
  assign fifo_rd_en    = m_axis_tvalid && m_axis_tready;
  assign m_axis_tlast  = fifo_dout[16];  // MSB
  assign m_axis_tdata  = fifo_dout[15:0];  // Lower 16 bits

endmodule

//!  Experimental Non-Blocking SPI Transfer API
use crate::{
    self as mynewt,
    result::*,
    hw::hal,
    kernel::os,
    NULL, Ptr, Strn,
};
use mynewt_macros::{
    init_strn,
};

//  TODO: Remove SPI settings for ST7789 display controller
const DISPLAY_SPI: i32  =  0;  //  Mynewt SPI port 0
const DISPLAY_CS: i32   = 25;  //  LCD_CS (P0.25): Chip select
const DISPLAY_DC: i32   = 18;  //  LCD_RS (P0.18): Clock/data pin (CD)
const DISPLAY_RST: i32  = 26;  //  LCD_RESET (P0.26): Display reset
const DISPLAY_HIGH: i32 = 23;  //  LCD_BACKLIGHT_{LOW,MID,HIGH} (P0.14, 22, 23): Backlight (active low)

const SPI_NUM: i32 = DISPLAY_SPI;
const SPI_SS_PIN: i32 = DISPLAY_CS;

/// SPI settings for ST7789 display controller
static mut SPI_SETTINGS: hal::hal_spi_settings = hal::hal_spi_settings {
    data_order: hal::HAL_SPI_MSB_FIRST as u8,
    data_mode:  hal::HAL_SPI_MODE3 as u8,  //  SPI must be used in mode 3. Mode 0 (the default) won't work.
    baudrate:   8000,  //  In kHZ. Use SPI at 8MHz (the fastest clock available on the nRF52832) because otherwise refreshing will be super slow.
    word_size:  hal::HAL_SPI_WORD_SIZE_8BIT as u8,
};

/// Non-blocking SPI transfer callback parameter
struct spi_cb_arg {
    transfers: i32,
    txlen: i32,
    tx_rx_bytes: u32,
}

/// Non-blocking SPI transfer callback values
static mut spi_cb_obj: spi_cb_arg = spi_cb_arg {
    transfers: 0,
    txlen: 0,
    tx_rx_bytes: 0,
};

/// Semaphore that is signalled for every completed SPI request
static mut SPI_SEM: os::os_sem = fill_zero!(os::os_sem);
static mut SPI_DATA_QUEUE: os::os_mqueue = fill_zero!(os::os_mqueue);
static mut SPI_EVENT_QUEUE: os::os_eventq = fill_zero!(os::os_eventq);

/// Callout that is invoked when non-blocking SPI transfer is completed
static mut spi_callout: os::os_callout = fill_zero!(os::os_callout);

///  Storage for SPI Task: Mynewt task object will be saved here.
static mut SPI_TASK: os::os_task = fill_zero!(os::os_task);
///  Stack space for SPI Task, initialised to 0.
static mut SPI_TASK_STACK: [os::os_stack_t; SPI_TASK_STACK_SIZE] = 
    [0; SPI_TASK_STACK_SIZE];
///  Size of the stack (in 4-byte units). Previously `OS_STACK_ALIGN(256)`  
const SPI_TASK_STACK_SIZE: usize = 256;

/// Init non-blocking SPI transfer
pub fn spi_noblock_init() -> MynewtResult<()> {
    unsafe { hal::hal_spi_disable(SPI_NUM) };

    let rc = unsafe { hal::hal_spi_config(SPI_NUM, &mut SPI_SETTINGS) };
    assert_eq!(rc, 0, "spi config fail");  //  TODO: Map to MynewtResult

    let arg = unsafe { core::mem::transmute(&mut spi_cb_obj) };
    let rc = unsafe { hal::hal_spi_set_txrx_cb(SPI_NUM, Some(spi_noblock_handler), arg) };
    assert_eq!(rc, 0, "spi cb fail");  //  TODO: Map to MynewtResult

    let rc = unsafe { hal::hal_spi_enable(SPI_NUM) };
    assert_eq!(rc, 0, "spi enable fail");  //  TODO: Map to MynewtResult

    let rc = unsafe { hal::hal_gpio_init_out(SPI_SS_PIN, 1) };
    assert_eq!(rc, 0, "gpio fail");  //  TODO: Map to MynewtResult

    unsafe { os::os_eventq_init(&mut SPI_EVENT_QUEUE) };

    let rc = unsafe { os::os_mqueue_init(&mut SPI_DATA_QUEUE, Some(spi_event_callback), NULL) };
    assert_eq!(rc, 0, "mqueue fail");  //  TODO: Map to MynewtResult

    let rc = unsafe { os::os_sem_init(&mut SPI_SEM, 0) };  //  Init to 0 tokens, so caller will block until SPI request is completed.
    assert_eq!(rc, 0, "sem fail");  //  TODO: Map to MynewtResult

    os::task_init(                //  Create a new task and start it...
        unsafe { &mut SPI_TASK }, //  Task object will be saved here
        &init_strn!( "spi" ),     //  Name of task
        Some( spi_task_func ),    //  Function to execute when task starts
        NULL,  //  Argument to be passed to above function
        10,    //  Task priority: highest is 0, lowest is 255 (main task is 127)
        os::OS_WAIT_FOREVER as u32,     //  Don't do sanity / watchdog checking
        unsafe { &mut SPI_TASK_STACK }, //  Stack space for the task
        SPI_TASK_STACK_SIZE as u16      //  Size of the stack (in 4-byte units)
    ) ? ;                               //  `?` means check for error

    //  Init the callout to handle completed SPI transfers.
    unsafe {
        os::os_callout_init(
            &mut spi_callout, 
            os::eventq_dflt_get() ? , 
            Some(spi_noblock_callback), 
            core::ptr::null_mut()
        )
    };
    Ok(())
}

/// Enqueue request for non-blocking SPI write. Returns without waiting for write to complete.
#[cfg(feature = "spi_noblock")]
pub fn spi_noblock_write(words: &[u8]) -> MynewtResult<()> {
    //  Add to request queue. Make a copy of the data to be sent.

    //  struct os_mbuf *semihost_mbuf = os_msys_get_pkthdr(length, 0);
    //  if (!semihost_mbuf) { return; }  //  If out of memory, quit.

    //  Append the data to the mbuf chain.  This may increase the numbere of mbufs in the chain.
    //  rc = os_mbuf_append(semihost_mbuf, buffer, length);
    //  if (rc) { return; }  //  If out of memory, quit.

    //  rc = os_mqueue_put(&SPI_DATA_QUEUE, &SPI_EVENT_QUEUE, om);
    //  if (rc) { return; }  //  If out of memory, quit.

    Ok(())
}

/// Perform non-blocking SPI write.  Returns without waiting for write to complete.
#[cfg(feature = "spi_noblock")]
fn internal_spi_noblock_write(txbuffer: Ptr, txlen: i32) -> MynewtResult<()> {
    unsafe { spi_cb_obj.txlen = txlen };
    //  Set the SS Pin to low to start the transfer.
    unsafe { hal::hal_gpio_write(SPI_SS_PIN, 0) };

    //  Write the SPI data.
    let rc = unsafe { hal::hal_spi_txrx_noblock(
        SPI_NUM, 
        txbuffer, //  TX Buffer
        NULL,     //  RX Buffer (don't receive)        
        txlen) };
    assert_eq!(rc, 0, "spi fail");  //  TODO: Map to MynewtResult
    Ok(())
}

/// Callback for the touch event that is triggered when a touch is detected
extern "C" fn spi_event_callback(_event: *mut os::os_event) {
    loop {
        //  Get the next data packet.
        let om = unsafe { os::os_mqueue_get(&mut SPI_DATA_QUEUE) };
        if om.is_null() { break; }

        //  TODO: Write the data packet

        //  Wait for spi_noblock_handler() to signal that SPI request has been completed.
        let timeout = 1000;
        let OS_TICKS_PER_SEC = 1000;
        unsafe { os::os_sem_pend(&mut SPI_SEM, timeout * OS_TICKS_PER_SEC / 1000) };

        //  Free the data packet.
        unsafe { os::os_mbuf_free_chain(om) };
    }
}

// Process each event posted to our eventq.  When there are no events to process, sleep until one arrives.
extern "C" fn spi_task_func(_arg: Ptr) {
    loop {
        os::eventq_run(
            unsafe { &mut SPI_EVENT_QUEUE }
        ).expect("eventq fail");
    }
}

/// Called by interrupt handler after Non-blocking SPI transfer has completed
extern "C" fn spi_noblock_handler(_arg: *mut core::ffi::c_void, _len: i32) {
    //  Set SS Pin to high to stop the transfer.
    unsafe { hal::hal_gpio_write(SPI_SS_PIN, 1) };

    //  Trigger the callout to transmit next SPI request.
    unsafe { os::os_callout_reset(&mut spi_callout, 0) };

    //  Signal to internal_spi_noblock_write() that SPI request has been completed.
    //  os_error_t rc = os_sem_release(&SPI_SEM);
    //  assert(rc == OS_OK);
}

/// Callout after Non-blocking SPI transfer as completed
extern "C" fn spi_noblock_callback(_ev: *mut os::os_event) {
    //  TODO: Transmit the next queued SPI request.
    //  internal_spi_noblock_write(txbuffer: *mut core::ffi::c_void, txlen: i32);
}

/* mbuf
    static struct os_mbuf *semihost_mbuf = NULL;

    void console_flush(void) {
        //  Flush output buffer to the console log.  This will be slow.
        if (!log_enabled) { return; }       //  Skip if log not enabled.
        if (!semihost_mbuf) { return; }     //  Buffer is empty, nothing to write.
        if (os_arch_in_isr()) { return; }   //  Don't flush if we are called during an interrupt.

        //  Swap mbufs first to prevent concurrency problems.
        struct os_mbuf *old = semihost_mbuf;
        semihost_mbuf = NULL;

        struct os_mbuf *m = old;
        while (m) {  //  For each mbuf in the chain...
            const unsigned char *data = OS_MBUF_DATA(m, const unsigned char *);  //  Fetch the data.
            int size = m->om_len;                         //  Fetch the size.
            semihost_write(SEMIHOST_HANDLE, data, size);  //  Write the data to Semihosting output.
            m = m->om_next.sle_next;                      //  Fetch next mbuf in the chain.
        }
        if (old) { os_mbuf_free_chain(old); }  //  Deallocate the old chain.
    }

    void console_buffer(const char *buffer, unsigned int length) {
        //  Append "length" number of bytes from "buffer" to the output buffer.
    #ifdef DISABLE_SEMIHOSTING  //  If Arm Semihosting is disabled...
        return;                 //  Don't write debug messages.
    #else                       //  If Arm Semihosting is enabled...
        int rc;
        if (!log_enabled) { return; }           //  Skip if log not enabled.
        if (!debugger_connected()) { return; }  //  If debugger is not connected, quit.
        if (!semihost_mbuf) {                   //  Allocate mbuf if not already allocated.
            semihost_mbuf = os_msys_get_pkthdr(length, 0);
            if (!semihost_mbuf) { return; }  //  If out of memory, quit.
        }
        //  Limit the buffer size.  Quit if too big.
        if (os_mbuf_len(semihost_mbuf) + length >= OUTPUT_BUFFER_SIZE) { return; }
        //  Append the data to the mbuf chain.  This may increase the numbere of mbufs in the chain.
        rc = os_mbuf_append(semihost_mbuf, buffer, length);
        if (rc) { return; }  //  If out of memory, quit.
    #endif  //  DISABLE_SEMIHOSTING
    }
*/

/* mqueue
    uint32_t pkts_rxd;
    struct os_mqueue SPI_DATA_QUEUE;
    struct os_eventq SPI_EVENT_QUEUE;

    // Removes each packet from the receive queue and processes it.
    void
    process_rx_data_queue(void)
    {
        struct os_mbuf *om;

        while ((om = os_mqueue_get(&SPI_DATA_QUEUE)) != NULL) {
            ++pkts_rxd;
            os_mbuf_free_chain(om);
        }
    }

    // Called when a packet is received.
    int
    my_task_rx_data_func(struct os_mbuf *om)
    {
        int rc;

        // Enqueue the received packet and wake up the listening task.
        rc = os_mqueue_put(&SPI_DATA_QUEUE, &SPI_EVENT_QUEUE, om);
        if (rc != 0) {
            return -1;
        }

        return 0;
    }

    void
    my_task_handler(void *arg)
    {
        struct os_event *ev;
        struct os_callout_func *cf;
        int rc;

        // Initialize eventq
        os_eventq_init(&SPI_EVENT_QUEUE);

        // Initialize mqueue
        os_mqueue_init(&SPI_DATA_QUEUE, NULL);

        // Process each event posted to our eventq.  When there are no events to process, sleep until one arrives.
        while (1) {
            os_eventq_run(&SPI_EVENT_QUEUE);
        }
    }
*/

/* Non-Blocking SPI Transfer in Mynewt OS

    //  The spi txrx callback
    struct spi_cb_arg {
        int transfers;
        int txlen;
        uint32_t tx_rx_bytes;
    };
    struct spi_cb_arg spi_cb_obj;
    void *spi_cb_arg;
    ...
    void spi_noblock_handler(void *arg, int len) {
        int i;
        struct spi_cb_arg *cb;
        hal_gpio_write(SPI_SS_PIN, 1);
        if (spi_cb_arg) {
            cb = (struct spi_cb_arg *)arg;
            assert(len == cb->txlen);
            ++cb->transfers;
        }
        ++g_spi_xfr_num;
    }
    ...
    //  Non-blocking SPI transfer
    hal_spi_disable(SPI_NUM);
    spi_cb_arg = &spi_cb_obj;
    spi_cb_obj.txlen = 32;
    hal_spi_set_txrx_cb(SPI_NUM, spi_noblock_handler, spi_cb_arg);
    hal_spi_enable(SPI_NUM);
    ...
    hal_gpio_write(SPI_SS_PIN, 0);
    rc = hal_spi_txrx_noblock(SPI_NUM, g_spi_tx_buf, g_spi_rx_buf,
                                spi_cb_obj.txlen);
    assert(!rc);
*/
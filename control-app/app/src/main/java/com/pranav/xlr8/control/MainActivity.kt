package com.pranav.xlr8.control

import android.content.Context
import android.content.pm.ActivityInfo
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Bundle
import android.view.View
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.pranav.xlr8.control.ui.theme.ControlXLR8Theme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import kotlin.math.roundToInt


// ============================================================
// UDP SETTINGS
// ============================================================

private const val BOT_IP = "192.168.4.1"
private const val CONTROL_PORT = 5000

private const val PING_INTERVAL = 3000L
private const val PING_TIMEOUT = 1500

private const val MAX_FAILED_CHECKS = 3


// ============================================================
// COLORS
// ============================================================

private val Background = Color(0xFF070A0C)

private val HeaderGreen1 = Color(0xFF13261A)
private val HeaderGreen2 = Color(0xFF1A3421)

private val Panel = Color(0xFF101519)
private val Panel2 = Color(0xFF141B20)

private val Border = Color(0xFF28363D)

private val TextSecondary = Color(0xFF89979D)

private val Yellow = Color(0xFFE4D34A)

private val SpeedBlue1 = Color(0xFF5C9AB7)
private val SpeedBlue2 = Color(0xFF315D70)

private val SteeringGreen1 = Color(0xFFD0EBAF)
private val SteeringGreen2 = Color(0xFF91B374)

private val Purple = Color(0xFF71346F)

private val BrakeOrange1 = Color(0xFFF27A32)
private val BrakeOrange2 = Color(0xFFA43F20)
private val BrakeRed = Color(0xFFFF4935)


// ============================================================
// CONTROL STATE
//
// EXACTLY represents the data sent to the ESP32.
// ============================================================

private data class ControlState(
    val speed: Int = 0,
    val steering: Int = 0,
    val mode: Int = 0,
    val brake: Int = 0,
    val direction: Int = 0
)


// ============================================================
// MAIN ACTIVITY
// ============================================================

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        requestedOrientation =
            ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE

        enableEdgeToEdge()

        hideSystemUI()

        setContent {
            ControlXLR8Theme {
                XLR8App()
            }
        }
    }

    private fun hideSystemUI() {

        window.decorView.systemUiVisibility =
            View.SYSTEM_UI_FLAG_FULLSCREEN or
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
    }
}


// ============================================================
// MAIN APP
// ============================================================

@Composable
private fun XLR8App() {

    val context = LocalContext.current

    var connected by remember {
        mutableStateOf(false)
    }

    var checking by remember {
        mutableStateOf(true)
    }

    var retryCounter by remember {
        mutableIntStateOf(0)
    }


    // ========================================================
    // CONNECTION MONITOR
    //
    // PING every 3 seconds.
    //
    // A single lost PONG does NOT disconnect.
    //
    // 3 consecutive failures = disconnected.
    // ========================================================

    LaunchedEffect(retryCounter) {

        checking = true

        connected = false


        // ====================================================
        // INITIAL CONNECTION
        // ====================================================

        var initialConnection = false

        repeat(3) {

            if (!initialConnection) {

                initialConnection =
                    checkBotConnection(context)

                if (!initialConnection) {
                    delay(300)
                }
            }
        }


        connected = initialConnection

        checking = false


        // ====================================================
        // CONTINUOUS MONITOR
        // ====================================================

        if (initialConnection) {

            var failedChecks = 0

            while (true) {

                delay(PING_INTERVAL)


                val alive =
                    checkBotConnection(context)


                if (alive) {

                    // ----------------------------------------
                    // PONG RECEIVED
                    // ----------------------------------------

                    failedChecks = 0

                    connected = true

                } else {

                    // ----------------------------------------
                    // PONG MISSED
                    // ----------------------------------------

                    failedChecks++


                    // Do not disconnect immediately.
                    //
                    // Failure 1 -> ignore
                    // Failure 2 -> ignore
                    // Failure 3 -> disconnect

                    if (failedChecks >= MAX_FAILED_CHECKS) {

                        connected = false

                        break
                    }
                }
            }
        }
    }


    // ========================================================
    // SCREEN
    // ========================================================

    if (connected) {

        CarController(
            onDisconnected = {
                connected = false
            }
        )

    } else {

        StartScreen(
            checkingConnection = checking,

            onRetry = {
                retryCounter++
            }
        )
    }
}


// ============================================================
// START SCREEN
// ============================================================

@Composable
private fun StartScreen(
    checkingConnection: Boolean,
    onRetry: () -> Unit
) {

    Box(
        modifier =
            Modifier
                .fillMaxSize()
                .background(Background),

        contentAlignment =
            Alignment.Center
    ) {

        Column(
            horizontalAlignment =
                Alignment.CenterHorizontally
        ) {

            Image(
                painter =
                    painterResource(
                        id = R.drawable.xlr8_logo
                    ),

                contentDescription =
                    "XLR8 Logo",

                modifier =
                    Modifier.size(170.dp)
            )


            Spacer(
                modifier =
                    Modifier.height(22.dp)
            )


            Text(
                text = "XLR8",

                color = Yellow,

                fontSize = 30.sp,

                fontWeight =
                    FontWeight.ExtraBold,

                letterSpacing = 5.sp
            )


            Spacer(
                modifier =
                    Modifier.height(8.dp)
            )


            if (checkingConnection) {

                Text(
                    text = "CONNECTING TO BOT...",

                    color = TextSecondary,

                    fontSize = 11.sp,

                    fontWeight =
                        FontWeight.Bold,

                    letterSpacing = 2.sp
                )

            } else {

                Text(
                    text = "BOT NOT FOUND",

                    color = BrakeRed,

                    fontSize = 11.sp,

                    fontWeight =
                        FontWeight.Bold,

                    letterSpacing = 2.sp
                )


                Spacer(
                    modifier =
                        Modifier.height(18.dp)
                )


                Box(
                    modifier =
                        Modifier
                            .clip(
                                RoundedCornerShape(12.dp)
                            )
                            .background(Panel2)
                            .border(
                                1.dp,
                                Border,
                                RoundedCornerShape(12.dp)
                            )
                            .clickable {
                                onRetry()
                            }
                            .padding(
                                horizontal = 24.dp,
                                vertical = 12.dp
                            )
                ) {

                    Text(
                        text = "RETRY",

                        color = Color.White,

                        fontSize = 10.sp,

                        fontWeight =
                            FontWeight.Bold,

                        letterSpacing = 1.5.sp
                    )
                }
            }
        }
    }
}


// ============================================================
// WI-FI BINDING HELPER
//
// Binds UDP sockets to Wi-Fi network interface so packets
// reach 192.168.4.1 directly even when Mobile Data is ON.
// ============================================================

private fun bindSocketToWifi(
    context: Context,
    socket: DatagramSocket
) {

    try {

        val cm =
            context.applicationContext
                .getSystemService(
                    Context.CONNECTIVITY_SERVICE
                ) as? ConnectivityManager


        if (cm != null) {

            val wifiNetwork =
                cm.allNetworks.firstOrNull { network ->

                    val caps =
                        cm.getNetworkCapabilities(
                            network
                        )

                    caps != null &&
                            caps.hasTransport(
                                NetworkCapabilities.TRANSPORT_WIFI
                            )
                }


            wifiNetwork?.bindSocket(
                socket
            )
        }

    } catch (
        e: Exception
    ) {

        e.printStackTrace()
    }
}


// ============================================================
// CONNECTION CHECK
// ============================================================

private suspend fun checkBotConnection(
    context: Context
): Boolean {

    return withContext(Dispatchers.IO) {

        var socket: DatagramSocket? = null

        try {

            socket =
                DatagramSocket()

            bindSocketToWifi(
                context,
                socket
            )

            socket.soTimeout =
                PING_TIMEOUT


            val address =
                InetAddress.getByName(
                    BOT_IP
                )


            // =================================================
            // SEND PING
            // =================================================

            val ping =
                "PING".toByteArray()


            val packet =
                DatagramPacket(
                    ping,
                    ping.size,
                    address,
                    CONTROL_PORT
                )


            socket.send(packet)


            // =================================================
            // WAIT FOR PONG
            // =================================================

            val buffer =
                ByteArray(32)


            val response =
                DatagramPacket(
                    buffer,
                    buffer.size
                )


            socket.receive(response)


            // =================================================
            // READ MESSAGE
            // =================================================

            val message =
                String(
                    response.data,
                    0,
                    response.length
                ).trim()


            message == "PONG"

        } catch (
            e: Exception
        ) {

            false

        } finally {

            socket?.close()
        }
    }
}


// ============================================================
// UDP CONTROL SENDER
// Sends control packets sequentially on a dedicated single
// thread while ensuring Wi-Fi network socket binding.
// ============================================================

private object UdpControlSender {

    private val executor =
        java.util.concurrent.Executors.newSingleThreadExecutor()

    fun send(
        context: Context,
        packetData: ByteArray
    ) {

        val appContext =
            context.applicationContext


        executor.execute {

            var socket: DatagramSocket? = null

            try {

                socket =
                    DatagramSocket()

                bindSocketToWifi(
                    appContext,
                    socket
                )

                val address =
                    InetAddress.getByName(
                        BOT_IP
                    )

                val packet =
                    DatagramPacket(
                        packetData,
                        packetData.size,
                        address,
                        CONTROL_PORT
                    )

                socket.send(packet)

            } catch (
                e: Exception
            ) {

                e.printStackTrace()

            } finally {

                socket?.close()
            }
        }
    }
}


// ============================================================
// UDP BINARY CONTROL PACKET
//
// EXACTLY 6 BYTES
//
// byte 0 = speed
// byte 1 = steering
// byte 2 = mode
// byte 3 = brake
// byte 4 = direction
// byte 5 = sequence
// ============================================================

private fun sendControlPacket(
    context: Context,
    state: ControlState,
    sequence: Int
) {

    val speed =
        state.speed.coerceIn(
            -100,
            100
        )


    val steering =
        state.steering.coerceIn(
            -100,
            100
        )


    val direction =
        when {

            speed > 0 ->
                1

            speed < 0 ->
                2

            else ->
                0
        }


    val packetData =
        byteArrayOf(

            speed.toByte(),

            steering.toByte(),

            state.mode.toByte(),

            state.brake.toByte(),

            direction.toByte(),

            sequence.toByte()
        )


    UdpControlSender.send(
        context,
        packetData
    )
}


// ============================================================
// MAIN CONTROLLER
// ============================================================

@Composable
private fun CarController(
    onDisconnected: () -> Unit
) {

    val context =
        LocalContext.current

    var speed by remember {
        mutableIntStateOf(0)
    }

    var steering by remember {
        mutableIntStateOf(0)
    }

    var mode by remember {
        mutableIntStateOf(0)
    }

    var brake by remember {
        mutableIntStateOf(0)
    }

    val sequence = remember {
        java.util.concurrent.atomic.AtomicInteger(0)
    }


    // ========================================================
    // SEND CURRENT STATE
    // ========================================================

    fun sendCurrentState() {

        val seq =
            sequence.incrementAndGet() and 255


        val state =
            ControlState(
                speed = speed,
                steering = steering,
                mode = mode,
                brake = brake
            )


        sendControlPacket(
            context,
            state,
            seq
        )
    }


    // ========================================================
    // SPEED
    // ========================================================

    fun updateSpeed(
        value: Int
    ) {

        speed =
            value.coerceIn(
                -100,
                100
            )

        sendCurrentState()
    }


    // ========================================================
    // STEERING
    // ========================================================

    fun updateSteering(
        value: Int
    ) {

        steering =
            value.coerceIn(
                -100,
                100
            )

        sendCurrentState()
    }


    // ========================================================
    // MODE
    // ========================================================

    fun updateMode(
        value: Int
    ) {

        mode = value

        sendCurrentState()
    }


    // ========================================================
    // BRAKE
    // ========================================================

    fun updateBrake(
        value: Int
    ) {

        brake = value

        sendCurrentState()
    }


    // ========================================================
    // UI
    // ========================================================

    Box(
        modifier =
            Modifier
                .fillMaxSize()
                .background(Background)
                .padding(
                    start = 14.dp,
                    end = 14.dp,
                    top = 10.dp,
                    bottom = 10.dp
                )
    ) {

        Column(
            modifier =
                Modifier.fillMaxSize(),

            verticalArrangement =
                Arrangement.spacedBy(10.dp)
        ) {

            Header(
                speed = speed,

                mode =
                    when (mode) {

                        1 -> "LEFT"

                        2 -> "RIGHT"

                        else -> "NORMAL"
                    },

                steering = steering
            )


            Row(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .weight(1f),

                horizontalArrangement =
                    Arrangement.spacedBy(10.dp)
            ) {

                Spacer(
                    modifier =
                        Modifier.width(4.dp)
                )


                SpeedControl(
                    speed = speed,

                    onSpeedChange = {
                        updateSpeed(it)
                    },

                    modifier =
                        Modifier
                            .width(78.dp)
                            .fillMaxHeight()
                )


                Column(
                    modifier =
                        Modifier.weight(1f),

                    verticalArrangement =
                        Arrangement.spacedBy(12.dp)
                ) {

                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .weight(0.58f),

                        horizontalArrangement =
                            Arrangement.spacedBy(12.dp)
                    ) {

                        ModePanel(
                            mode = mode,

                            onModeChange = {
                                updateMode(it)
                            },

                            modifier =
                                Modifier
                                    .weight(0.38f)
                                    .fillMaxHeight()
                        )


                        SteeringPanel(
                            steering = steering,

                            onSteeringChange = {
                                updateSteering(it)
                            },

                            modifier =
                                Modifier
                                    .weight(0.62f)
                                    .fillMaxHeight()
                        )
                    }


                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .weight(0.42f),

                        horizontalArrangement =
                            Arrangement.spacedBy(12.dp)
                    ) {

                        Spacer(
                            modifier =
                                Modifier.weight(0.62f)
                        )


                        BrakePanel(
                            active =
                                brake == 1,

                            onPressed = {
                                updateBrake(1)
                            },

                            onReleased = {
                                updateBrake(0)
                            },

                            modifier =
                                Modifier
                                    .weight(0.38f)
                                    .fillMaxHeight()
                        )
                    }
                }
            }
        }
    }
}


// ============================================================
// HEADER
// ============================================================

@Composable
private fun Header(
    speed: Int,
    mode: String,
    steering: Int
) {

    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .height(72.dp)
                .clip(
                    RoundedCornerShape(20.dp)
                )
                .background(
                    Brush.linearGradient(
                        listOf(
                            HeaderGreen1,
                            HeaderGreen2
                        )
                    )
                )
                .border(
                    1.dp,
                    Color(0xFF2B4A34),
                    RoundedCornerShape(20.dp)
                )
                .padding(
                    horizontal = 18.dp
                ),

        verticalAlignment =
            Alignment.CenterVertically
    ) {

        Column(
            modifier =
                Modifier.width(90.dp)
        ) {

            Text(
                text = "XLR8",

                color = Yellow,

                fontSize = 19.sp,

                fontWeight =
                    FontWeight.ExtraBold,

                letterSpacing = 2.sp
            )


            Text(
                text = "CONTROL",

                color = TextSecondary,

                fontSize = 7.sp,

                letterSpacing = 2.sp
            )
        }


        HeaderStat(
            label = "SPEED",

            value =
                if (speed > 0)
                    "+$speed"
                else
                    speed.toString()
        )


        HeaderStat(
            label = "MODE",

            value = mode
        )


        HeaderStat(
            label = "STEERING",

            value =
                if (steering > 0)
                    "+$steering"
                else
                    steering.toString()
        )
    }
}


// ============================================================
// HEADER STAT
// ============================================================

@Composable
private fun RowScope.HeaderStat(
    label: String,
    value: String
) {

    Column(
        modifier =
            Modifier.weight(1f),

        horizontalAlignment =
            Alignment.CenterHorizontally,

        verticalArrangement =
            Arrangement.Center
    ) {

        Text(
            text = label,

            color = TextSecondary,

            fontSize = 8.sp,

            letterSpacing = 1.3.sp
        )


        Spacer(
            modifier =
                Modifier.height(2.dp)
        )


        Text(
            text = value,

            color = Yellow,

            fontSize = 19.sp,

            fontWeight =
                FontWeight.Bold
        )
    }
}


// ============================================================
// SPEED CONTROL
// ============================================================

@Composable
private fun SpeedControl(
    speed: Int,
    onSpeedChange: (Int) -> Unit,
    modifier: Modifier
) {

    Box(
        modifier =
            modifier
                .clip(
                    RoundedCornerShape(20.dp)
                )
                .background(
                    Brush.verticalGradient(
                        listOf(
                            Color(0xFF142B37),
                            Color(0xFF0E2029)
                        )
                    )
                )
                .border(
                    1.dp,
                    Color(0xFF294958),
                    RoundedCornerShape(20.dp)
                )
    ) {

        Text(
            text = "SPEED",

            modifier =
                Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 13.dp),

            color =
                Color(0xFFB8D2DD),

            fontSize = 9.sp,

            fontWeight =
                FontWeight.Bold,

            letterSpacing = 1.8.sp
        )


        BoxWithConstraints(
            modifier =
                Modifier
                    .fillMaxSize()
                    .padding(
                        top = 38.dp,
                        bottom = 22.dp
                    )
        ) {

            val usableHeight =
                maxHeight - 56.dp


            val normalized =
                ((speed + 100f) / 200f)
                    .coerceIn(
                        0f,
                        1f
                    )


            Box(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .pointerInput(Unit) {

                            detectDragGestures(

                                onDragStart = {
                                        position ->

                                    val value =
                                        (
                                                100f -
                                                        (
                                                                position.y /
                                                                        size.height
                                                                ) *
                                                        200f
                                                )
                                            .coerceIn(
                                                -100f,
                                                100f
                                            )
                                            .roundToInt()

                                    onSpeedChange(value)
                                },


                                onDrag = {
                                        change, _ ->

                                    val value =
                                        (
                                                100f -
                                                        (
                                                                change.position.y /
                                                                        size.height
                                                                ) *
                                                        200f
                                                )
                                            .coerceIn(
                                                -100f,
                                                100f
                                            )
                                            .roundToInt()

                                    onSpeedChange(value)

                                    change.consume()
                                },


                                onDragEnd = {

                                    onSpeedChange(0)
                                },


                                onDragCancel = {

                                    onSpeedChange(0)
                                }
                            )
                        }
            ) {

                // =================================================
                // TRACK
                // =================================================

                Box(
                    modifier =
                        Modifier
                            .width(7.dp)
                            .fillMaxHeight(0.82f)
                            .align(Alignment.Center)
                            .clip(
                                RoundedCornerShape(10.dp)
                            )
                            .background(
                                Color.White.copy(
                                    alpha = 0.07f
                                )
                            )
                )


                // =================================================
                // ZERO
                // =================================================

                Box(
                    modifier =
                        Modifier
                            .width(18.dp)
                            .height(2.dp)
                            .align(Alignment.Center)
                            .background(
                                Color.White.copy(
                                    alpha = 0.25f
                                )
                            )
                )


                // =================================================
                // ACTIVE TRACK
                // =================================================

                Box(
                    modifier =
                        Modifier
                            .width(7.dp)
                            .fillMaxHeight(0.41f)
                            .align(
                                if (speed >= 0)
                                    Alignment.BottomCenter
                                else
                                    Alignment.TopCenter
                            )
                            .clip(
                                RoundedCornerShape(10.dp)
                            )
                            .background(
                                Brush.verticalGradient(
                                    if (speed >= 0) {

                                        listOf(
                                            SpeedBlue1,
                                            SpeedBlue2
                                        )

                                    } else {

                                        listOf(
                                            BrakeRed,
                                            BrakeOrange2
                                        )
                                    }
                                )
                            )
                )


                // =================================================
                // THUMB
                // =================================================

                Box(
                    modifier =
                        Modifier
                            .size(56.dp)
                            .align(Alignment.Center)
                            .offset(
                                y =
                                    (
                                            (
                                                    0.5f -
                                                            normalized
                                                    ) *
                                                    usableHeight.value
                                            ).dp
                            )
                            .shadow(
                                8.dp,
                                CircleShape
                            )
                            .clip(CircleShape)
                            .background(
                                if (speed < 0) {

                                    Brush.radialGradient(
                                        listOf(
                                            BrakeRed,
                                            BrakeOrange2
                                        )
                                    )

                                } else {

                                    Brush.radialGradient(
                                        listOf(
                                            SpeedBlue1,
                                            SpeedBlue2
                                        )
                                    )
                                }
                            )
                            .border(
                                2.dp,

                                if (speed < 0)
                                    BrakeRed
                                else
                                    Color(0xFF76B0C8),

                                CircleShape
                            ),

                    contentAlignment =
                        Alignment.Center
                ) {

                    Text(
                        text =
                            if (speed > 0)
                                "+$speed"
                            else
                                speed.toString(),

                        color = Color.White,

                        fontSize = 15.sp,

                        fontWeight =
                            FontWeight.Bold
                    )
                }
            }
        }
    }
}


// ============================================================
// MODE PANEL
// ============================================================

@Composable
private fun ModePanel(
    mode: Int,
    onModeChange: (Int) -> Unit,
    modifier: Modifier
) {

    Box(
        modifier =
            modifier
                .clip(
                    RoundedCornerShape(20.dp)
                )
                .background(Panel)
                .border(
                    1.dp,
                    Border,
                    RoundedCornerShape(20.dp)
                )
    ) {

        Text(
            text = "DRIVE MODE",

            modifier =
                Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 14.dp),

            color = TextSecondary,

            fontSize = 9.sp,

            fontWeight =
                FontWeight.Bold,

            letterSpacing = 1.8.sp
        )


        Row(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .align(Alignment.Center)
                    .padding(
                        horizontal = 8.dp
                    ),

            horizontalArrangement =
                Arrangement.SpaceEvenly,

            verticalAlignment =
                Alignment.CenterVertically
        ) {

            HoldModeButton(
                text = "LEFT",

                active =
                    mode == 1,

                color = Purple,

                onPressed = {
                    onModeChange(1)
                },

                onReleased = {
                    onModeChange(0)
                }
            )


            NormalModeButton(
                active =
                    mode == 0,

                onClick = {
                    onModeChange(0)
                }
            )


            HoldModeButton(
                text = "RIGHT",

                active =
                    mode == 2,

                color = Purple,

                onPressed = {
                    onModeChange(2)
                },

                onReleased = {
                    onModeChange(0)
                }
            )
        }
    }
}


// ============================================================
// HOLD MODE BUTTON
// ============================================================

@Composable
private fun HoldModeButton(
    text: String,
    active: Boolean,
    color: Color,
    onPressed: () -> Unit,
    onReleased: () -> Unit
) {

    Box(
        modifier =
            Modifier
                .size(64.dp)
                .shadow(
                    if (active) 7.dp else 0.dp,
                    CircleShape
                )
                .clip(CircleShape)
                .background(
                    if (active)
                        color
                    else
                        color.copy(
                            alpha = 0.15f
                        )
                )
                .border(
                    1.5.dp,

                    if (active)
                        color
                    else
                        Border,

                    CircleShape
                )
                .pointerInput(Unit) {

                    awaitPointerEventScope {

                        while (true) {

                            awaitFirstDown()

                            onPressed()

                            waitForUpOrCancellation()

                            onReleased()
                        }
                    }
                },

        contentAlignment =
            Alignment.Center
    ) {

        Text(
            text = text,

            color =
                if (active)
                    Color.White
                else
                    Color.White.copy(
                        alpha = 0.45f
                    ),

            fontSize = 9.sp,

            fontWeight =
                FontWeight.Bold
        )
    }
}


// ============================================================
// NORMAL MODE
// ============================================================

@Composable
private fun NormalModeButton(
    active: Boolean,
    onClick: () -> Unit
) {

    Box(
        modifier =
            Modifier
                .size(64.dp)
                .shadow(
                    if (active) 7.dp else 0.dp,
                    CircleShape
                )
                .clip(CircleShape)
                .background(
                    if (active)
                        SteeringGreen1
                    else
                        SteeringGreen1.copy(
                            alpha = 0.15f
                        )
                )
                .border(
                    1.5.dp,

                    if (active)
                        Color(0xFFD3EDB7)
                    else
                        Border,

                    CircleShape
                )
                .clickable {
                    onClick()
                },

        contentAlignment =
            Alignment.Center
    ) {

        Text(
            text = "NORMAL",

            color =
                if (active)
                    Color(0xFF172015)
                else
                    Color.White.copy(
                        alpha = 0.45f
                    ),

            fontSize = 8.sp,

            fontWeight =
                FontWeight.Bold
        )
    }
}


// ============================================================
// STEERING
// ============================================================

@Composable
private fun SteeringPanel(
    steering: Int,
    onSteeringChange: (Int) -> Unit,
    modifier: Modifier
) {

    Box(
        modifier =
            modifier
                .clip(
                    RoundedCornerShape(20.dp)
                )
                .background(
                    Brush.verticalGradient(
                        listOf(
                            Panel2,
                            Panel
                        )
                    )
                )
                .border(
                    1.dp,
                    Border,
                    RoundedCornerShape(20.dp)
                )
    ) {

        Text(
            text = "STEERING",

            modifier =
                Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 14.dp),

            color = TextSecondary,

            fontSize = 9.sp,

            fontWeight =
                FontWeight.Bold,

            letterSpacing = 1.8.sp
        )


        Box(
            modifier =
                Modifier
                    .fillMaxWidth(0.90f)
                    .height(62.dp)
                    .align(Alignment.Center)
                    .clip(
                        RoundedCornerShape(50.dp)
                    )
                    .background(
                        Color(0xFF1D272C)
                    )
                    .border(
                        1.dp,
                        Color(0xFF34454D),
                        RoundedCornerShape(50.dp)
                    )
                    .pointerInput(Unit) {

                        detectDragGestures(

                            onDragStart = {
                                    position ->

                                val center =
                                    size.width / 2f

                                val value =
                                    (
                                            (
                                                    position.x -
                                                            center
                                                    ) /
                                                    center *
                                                    100f
                                            )
                                        .coerceIn(
                                            -100f,
                                            100f
                                        )
                                        .roundToInt()

                                onSteeringChange(value)
                            },


                            onDrag = {
                                    change, _ ->

                                val center =
                                    size.width / 2f

                                val value =
                                    (
                                            (
                                                    change.position.x -
                                                            center
                                                    ) /
                                                    center *
                                                    100f
                                            )
                                        .coerceIn(
                                            -100f,
                                            100f
                                        )
                                        .roundToInt()

                                onSteeringChange(value)

                                change.consume()
                            },


                            onDragEnd = {

                                onSteeringChange(0)
                            },


                            onDragCancel = {

                                onSteeringChange(0)
                            }
                        )
                    }
        ) {

            Text(
                text = "←",

                modifier =
                    Modifier
                        .align(
                            Alignment.CenterStart
                        )
                        .padding(
                            start = 12.dp
                        ),

                color =
                    Color(0xFF65757C),

                fontSize = 26.sp
            )


            Text(
                text = "→",

                modifier =
                    Modifier
                        .align(
                            Alignment.CenterEnd
                        )
                        .padding(
                            end = 12.dp
                        ),

                color =
                    Color(0xFF65757C),

                fontSize = 26.sp
            )


            Box(
                modifier =
                    Modifier
                        .width(2.dp)
                        .fillMaxHeight(0.55f)
                        .align(Alignment.Center)
                        .background(
                            Color.White.copy(
                                alpha = 0.12f
                            )
                        )
            )


            Box(
                modifier =
                    Modifier
                        .size(52.dp)
                        .align(Alignment.Center)
                        .offset(
                            x =
                                (
                                        steering *
                                                1.48f
                                        ).dp
                        )
                        .shadow(
                            8.dp,
                            CircleShape
                        )
                        .clip(CircleShape)
                        .background(
                            Brush.radialGradient(
                                listOf(
                                    SteeringGreen1,
                                    SteeringGreen2
                                )
                            )
                        )
                        .border(
                            2.dp,
                            Color(0xFFD3EDB7),
                            CircleShape
                        ),

                contentAlignment =
                    Alignment.Center
            ) {

                Text(
                    text =
                        if (steering > 0)
                            "+$steering"
                        else
                            steering.toString(),

                    color =
                        Color(0xFF172015),

                    fontSize = 15.sp,

                    fontWeight =
                        FontWeight.Bold
                )
            }
        }
    }
}


// ============================================================
// BRAKE
// ============================================================

@Composable
private fun BrakePanel(
    active: Boolean,
    onPressed: () -> Unit,
    onReleased: () -> Unit,
    modifier: Modifier
) {

    Box(
        modifier =
            modifier
                .clip(
                    RoundedCornerShape(20.dp)
                )
                .background(Panel)
                .border(
                    1.dp,
                    Border,
                    RoundedCornerShape(20.dp)
                )
    ) {

        Text(
            text = "BRAKE",

            modifier =
                Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 10.dp),

            color =
                if (active)
                    BrakeRed
                else
                    Color(0xFF99766F),

            fontSize = 8.sp,

            fontWeight =
                FontWeight.Bold,

            letterSpacing = 1.8.sp
        )


        Box(
            modifier =
                Modifier
                    .size(105.dp)
                    .align(Alignment.Center)
                    .shadow(
                        10.dp,
                        CircleShape
                    )
                    .clip(CircleShape)
                    .background(
                        Brush.radialGradient(
                            listOf(
                                if (active)
                                    BrakeRed
                                else
                                    BrakeOrange1,

                                BrakeOrange2
                            )
                        )
                    )
                    .border(
                        2.dp,

                        if (active)
                            Color(0xFFFF8978)
                        else
                            Color(0xFFFFA26E),

                        CircleShape
                    )
                    .pointerInput(Unit) {

                        awaitPointerEventScope {

                            while (true) {

                                awaitFirstDown()

                                onPressed()

                                waitForUpOrCancellation()

                                onReleased()
                            }
                        }
                    },

            contentAlignment =
                Alignment.Center
        ) {

            Column(
                horizontalAlignment =
                    Alignment.CenterHorizontally
            ) {

                Text(
                    text = "BRAKE",

                    color = Color.White,

                    fontSize = 16.sp,

                    fontWeight =
                        FontWeight.Bold,

                    letterSpacing = 1.8.sp
                )


                Spacer(
                    modifier =
                        Modifier.height(2.dp)
                )


                Text(
                    text = "HOLD",

                    color =
                        Color.White.copy(
                            alpha = 0.6f
                        ),

                    fontSize = 7.sp,

                    letterSpacing = 1.5.sp
                )
            }
        }
    }
}
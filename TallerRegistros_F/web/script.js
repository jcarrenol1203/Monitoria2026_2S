/* ==========================================================================
   STM32 CMSIS Interactive Simulator & Hub Logic
   ========================================================================== */

// Simulated Memory Registers
const registers = {
    RCC_AHB1ENR: 0x00000005, // Bits 0 (GPIOA) and 2 (GPIOC) set
    GPIOA_MODER: 0x00000400, // Bit 10 set (MODER5 = 01 -> Output)
    GPIOA_ODR:   0x00000020, // Bit 5 set (PA5 HIGH -> LED ON)
    GPIOC_MODER: 0x00000000, // Bits 27:26 = 00 (Input)
    GPIOC_IDR:   0x00002000  // Bit 13 set (PC13 HIGH -> Button released, active low)
};

// State variables
let isSimulationRunning = true;
let isButtonPressed = false;

// DOM Elements
document.addEventListener('DOMContentLoaded', () => {
    initRegisterGrid('RCC_AHB1ENR', [0, 2]);
    initRegisterGrid('GPIOA_MODER', [10, 11]);
    initRegisterGrid('GPIOA_ODR', [5]);
    initRegisterGrid('GPIOC_MODER', [26, 27]);
    initRegisterGrid('GPIOC_IDR', [13]);

    updateUI();
    setupEventListeners();
    setupTabs();
    setupGenerator();
    setupCalculator();
    
    // Start simulation loop (runs every 100ms)
    setInterval(mcuSimulationStep, 100);
});

// Initialize 32-bit visual grid
function initRegisterGrid(regKey, highlightBits = []) {
    const container = document.getElementById(`bits-${regKey}`);
    if (!container) return;
    
    container.innerHTML = '';
    const val = registers[regKey];

    // Create 32 bits from bit 31 down to bit 0
    for (let bit = 31; bit >= 0; bit--) {
        const bitState = (val >> bit) & 1;
        const bitDiv = document.createElement('div');
        bitDiv.className = `bit-cell ${bitState ? 'is-one' : ''}`;
        
        if (highlightBits.includes(bit)) {
            if (regKey.startsWith('GPIOA')) bitDiv.classList.add('highlight-pa5');
            if (regKey.startsWith('GPIOC')) bitDiv.classList.add('highlight-pc13');
            if (regKey.startsWith('RCC')) bitDiv.classList.add('highlight-pa5');
        }

        bitDiv.setAttribute('data-bit', bit);
        bitDiv.setAttribute('title', `Bit ${bit}`);
        bitDiv.innerHTML = `
            <span class="bit-idx">${bit}</span>
            <span class="bit-val">${bitState}</span>
        `;

        // Click on a bit to toggle manually
        bitDiv.addEventListener('click', () => {
            registers[regKey] ^= (1 << bit);
            updateUI();
        });

        container.appendChild(bitDiv);
    }
}

// Update 32-bit register display values and hex representation
function updateRegisterDisplay(regKey) {
    const hexSpan = document.getElementById(`hex-${regKey}`);
    if (hexSpan) {
        hexSpan.textContent = '0x' + (registers[regKey] >>> 0).toString(16).padStart(8, '0').toUpperCase();
    }

    const container = document.getElementById(`bits-${regKey}`);
    if (!container) return;

    const val = registers[regKey];
    const cells = container.querySelectorAll('.bit-cell');

    cells.forEach(cell => {
        const bit = parseInt(cell.getAttribute('data-bit'));
        const bitState = (val >> bit) & 1;
        const valSpan = cell.querySelector('.bit-val');
        
        if (valSpan) valSpan.textContent = bitState;
        cell.classList.toggle('is-one', bitState === 1);
    });
}

// Microcontroller Logic Execution Simulation
function mcuSimulationStep() {
    if (!isSimulationRunning) return;

    // 1. Check if RCC GPIO Clock is enabled
    const gpioaClock = (registers.RCC_AHB1ENR & (1 << 0)) !== 0;
    const gpiocClock = (registers.RCC_AHB1ENR & (1 << 2)) !== 0;

    // 2. Button hardware simulation -> GPIOC_IDR bit 13
    if (gpiocClock) {
        if (isButtonPressed) {
            registers.GPIOC_IDR &= ~(1 << 13); // Active Low: Pressed -> 0
        } else {
            registers.GPIOC_IDR |= (1 << 13);  // Released -> 1
        }
    }

    // 3. Simulated C Code Logic execution:
    // if ((GPIOC->IDR & (1 << 13)) == 0) -> PA5 = 1, else PA5 = 0
    if (gpioaClock && gpiocClock) {
        const moder5 = (registers.GPIOA_MODER >> 10) & 0b11;
        if (moder5 === 0b01) { // Pin PA5 configured as Output
            const isBtnActive = (registers.GPIOC_IDR & (1 << 13)) === 0;
            if (isBtnActive) {
                registers.GPIOA_ODR |= (1 << 5);  // Set PA5 HIGH
            } else {
                registers.GPIOA_ODR &= ~(1 << 5); // Clear PA5 LOW
            }
        }
    }

    updateUI();
}

// Update visual hardware UI
function updateUI() {
    // Update Hex Displays & Grids
    Object.keys(registers).forEach(reg => updateRegisterDisplay(reg));

    // Update Board LED LD2 (PA5) State
    const pa5State = (registers.GPIOA_ODR & (1 << 5)) !== 0;
    const ld2Bulb = document.getElementById('ld2-led');
    const ld2StateText = document.getElementById('ld2-state-text');
    const ledTag = document.getElementById('led-status-tag');

    if (pa5State) {
        ld2Bulb.classList.add('active');
        ld2StateText.textContent = '3.3V (HIGH - ON)';
        ld2StateText.style.color = '#00FF66';
        ledTag.textContent = 'ON (1)';
        ledTag.style.borderColor = '#00FF66';
        ledTag.style.color = '#00FF66';
    } else {
        ld2Bulb.classList.remove('active');
        ld2StateText.textContent = '0V (LOW - OFF)';
        ld2StateText.style.color = 'var(--text-muted)';
        ledTag.textContent = 'OFF (0)';
        ledTag.style.borderColor = 'var(--border-color)';
        ledTag.style.color = 'var(--text-muted)';
    }

    // Update B1 Button Display State
    const pc13State = (registers.GPIOC_IDR & (1 << 13)) !== 0;
    const b1StateText = document.getElementById('b1-state-text');
    const btnTag = document.getElementById('btn-status-tag');

    if (!pc13State) {
        b1StateText.textContent = '0V (GND - PRESIONADO)';
        b1StateText.style.color = '#00F0FF';
        btnTag.textContent = 'PRESIONADO (0)';
        btnTag.style.color = '#00F0FF';
    } else {
        b1StateText.textContent = '3.3V (VCC - SUELTO)';
        b1StateText.style.color = 'var(--text-muted)';
        btnTag.textContent = 'SUELTO (1)';
        btnTag.style.color = 'var(--text-muted)';
    }
}

// Setup Event Listeners
function setupEventListeners() {
    const userBtn = document.getElementById('user-b1-button');

    // Interactive button press events (mouse & touch)
    const pressHandler = () => {
        isButtonPressed = true;
        userBtn.classList.add('pressed');
        mcuSimulationStep();
    };

    const releaseHandler = () => {
        isButtonPressed = false;
        userBtn.classList.remove('pressed');
        mcuSimulationStep();
    };

    userBtn.addEventListener('mousedown', pressHandler);
    userBtn.addEventListener('mouseup', releaseHandler);
    userBtn.addEventListener('mouseleave', releaseHandler);
    userBtn.addEventListener('touchstart', (e) => { e.preventDefault(); pressHandler(); });
    userBtn.addEventListener('touchend', (e) => { e.preventDefault(); releaseHandler(); });

    // Simulation toggle button
    const runSimBtn = document.getElementById('btn-run-sim');
    runSimBtn.addEventListener('click', () => {
        isSimulationRunning = !isSimulationRunning;
        const btnText = document.getElementById('sim-btn-text');
        const mcuTag = document.getElementById('mcu-state-tag');

        if (isSimulationRunning) {
            btnText.textContent = 'Pausar Simulación';
            mcuTag.innerHTML = '<span class="dot pulse"></span> RUNNING';
        } else {
            btnText.textContent = 'Reanudar Simulación';
            mcuTag.innerHTML = 'PAUSED';
        }
    });

    // Reset MCU Button
    document.getElementById('btn-reset-sim').addEventListener('click', () => {
        registers.RCC_AHB1ENR = 0x00000005;
        registers.GPIOA_MODER = 0x00000400;
        registers.GPIOA_ODR   = 0x00000000;
        registers.GPIOC_MODER = 0x00000000;
        registers.GPIOC_IDR   = 0x00002000;
        isButtonPressed = false;
        updateUI();
    });

    // Theme Switcher
    const themeBtn = document.getElementById('theme-toggle');
    themeBtn.addEventListener('click', () => {
        document.body.classList.toggle('light-mode');
        themeBtn.textContent = document.body.classList.contains('light-mode') ? '☀️' : '🌙';
    });
}

// Tab Switching
function setupTabs() {
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');

    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const target = btn.getAttribute('data-tab');

            tabBtns.forEach(b => b.classList.remove('active'));
            tabContents.forEach(c => c.classList.remove('active'));

            btn.classList.add('active');
            document.getElementById(target).classList.add('active');
        });
    });
}

// Bit Operations Calculator
function setupCalculator() {
    const opSelect = document.getElementById('calc-op');
    const pinInput = document.getElementById('calc-pin');
    const resultCode = document.getElementById('calc-result-code');

    function recalculate() {
        const op = opSelect.value;
        const pin = parseInt(pinInput.value) || 0;

        let code = '';
        if (op === 'set') {
            const hex = (1 << pin) >>> 0;
            code = `REG |= (1 << ${pin}); // Mascara: 0x${hex.toString(16).padStart(8, '0').toUpperCase()}`;
        } else if (op === 'clear') {
            const mask = ~((0b11 << (pin * 2))) >>> 0;
            code = `REG &= ~(0b11 << (${pin} * 2)); // Mascara: 0x${mask.toString(16).padStart(8, '0').toUpperCase()}`;
        } else if (op === 'toggle') {
            const hex = (1 << pin) >>> 0;
            code = `REG ^= (1 << ${pin}); // Mascara: 0x${hex.toString(16).padStart(8, '0').toUpperCase()}`;
        }
        resultCode.textContent = code;
    }

    opSelect.addEventListener('change', recalculate);
    pinInput.addEventListener('input', recalculate);
    recalculate();
}

// CMSIS Code Generator
function setupGenerator() {
    const portSelect = document.getElementById('gen-port');
    const pinInput  = document.getElementById('gen-pin');
    const modeSelect = document.getElementById('gen-mode');
    const pupdSelect = document.getElementById('gen-pupd');
    const outputCode = document.getElementById('gen-output-code');

    function updateGenerator() {
        const port = portSelect.value;
        const pin = parseInt(pinInput.value) || 0;
        const mode = modeSelect.value;
        const pupd = pupdSelect.value;

        const clockBit = port === 'GPIOA' ? 0 : port === 'GPIOB' ? 1 : port === 'GPIOC' ? 2 : 3;

        let code = `// --- Configuración CMSIS para ${port} Pin ${pin} ---\n`;
        code += `// 1. Activar reloj en RCC_AHB1ENR\n`;
        code += `RCC->AHB1ENR |= (1 << ${clockBit}); // Activar ${port}\n\n`;

        code += `// 2. Configurar modo en ${port}->MODER\n`;
        code += `${port}->MODER &= ~(0b11 << (${pin} * 2)); // Limpiar bits\n`;
        if (mode !== '00') {
            code += `${port}->MODER |=  (0b${mode} << (${pin} * 2)); // Asignar modo 0b${mode}\n`;
        }

        if (pupd !== '00') {
            code += `\n// 3. Configurar Pull-Up/Pull-Down en ${port}->PUPDR\n`;
            code += `${port}->PUPDR &= ~(0b11 << (${pin} * 2));\n`;
            code += `${port}->PUPDR |=  (0b${pupd} << (${pin} * 2));\n`;
        }

        outputCode.textContent = code;
    }

    [portSelect, pinInput, modeSelect, pupdSelect].forEach(el => el.addEventListener('change', updateGenerator));
    pinInput.addEventListener('input', updateGenerator);
    updateGenerator();
}

// Clipboard copy helper
function copyCode(elementId) {
    const codeEl = document.getElementById(elementId);
    if (!codeEl) return;

    navigator.clipboard.writeText(codeEl.textContent).then(() => {
        alert('¡Código copiado al portapapeles!');
    }).catch(err => {
        console.error('Error al copiar:', err);
    });
}

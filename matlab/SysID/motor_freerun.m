function motor_freerun()
    clc;
    clear;
    %% Input Data
    Motor_LEFT =   ['Motor - Free Run/LEFT_3.csv'];% 'Motor - Free Run/LEFT_2.csv'];
    Motor_RIGHT = ['Motor - Free Run/RIGHT_1.csv'; 'Motor - Free Run/RIGHT_2.csv'];

    SysID(Motor_LEFT, 'LEFT');
    
end
function [R, Llsq, K] = SysID(data, Title)
    %% Params
    global T N Vmax GearRatio;
    
    T = 1/100;
    N = 23;

    Vmax = 12;
    DutyMax = 65535;
    CPM = 330;
    ADC_GAIN = 10;
    ADC_MAX  = 512;
    ADC_VREF = 5;
    ADC_RESISTOR = 0.1;

    GearRatio = 1/30;

    LOWDATA = 200;
    HIGHDATA = 4000;

    t = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));
    
    V = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));
    
    THETA = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));
    DTHETA = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));
    DDTHETA = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));

    CURRENT = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));
    DCURRENT = zeros(HIGHDATA - LOWDATA - 2*N + 1, size(data,1));
    
    for i = 1:size(data,1)
        %% Get Data
        [t_tmp,v_tmp,ENC,ADC] = import_motor_free(data(i, :), LOWDATA, HIGHDATA);

        %% Format Data
        theta_tmp = ENC * 2 * pi/CPM;
        cur_tmp = ADC*ADC_VREF/ADC_GAIN/ADC_MAX/ADC_RESISTOR;

        %% Get Derivatives
        [theta_tmp, dtheta_tmp, ddtheta_tmp] = quadraticSavitzkyGolay(theta_tmp',T,N);  
        V(:, i) = v_tmp(N+1:end-N);
        THETA(:, i) = theta_tmp(N+1:end-N)';
        DTHETA(:, i) = dtheta_tmp(N+1:end-N)';
        DDTHETA(:, i) = ddtheta_tmp(N+1:end-N)';

        [cur_tmp,dcur_tmp, ~] = quadraticSavitzkyGolay(cur_tmp',T,N);  
        CURRENT(:, i) = cur_tmp(N+1:end-N)';
        DCURRENT(:, i) = dcur_tmp(N+1:end-N)';

        t(:, i) = t_tmp(N+1:end-N);
        t(:, i) = t(:, i) - t(1, i);
    end

    %% Parameter Estimation
    for i = 1:size(data, 1)
        [Llsq(i), Rlsq(i), Klsq(i)] = motor_free_lsqf(V(:, i), CURRENT(:, i), DCURRENT(:, i), DTHETA(:, i), DDTHETA(:, i));
    end
    
    L0 = mean(Llsq)
    R0 = mean(Rlsq)
    K0 = mean(Klsq)
    
    
    
    %% Save Data
    figure;
    subplot(6,1,1);
    plot(t, V);
    title([Title, ': V_{RMS}']);
    ylabel('Voltage (V)');
    xlim([0 max(t(:))]);
    grid on;

    subplot(6, 1,2);
    plot(t, THETA);
    title([Title, ': Position']);
    ylabel('Position (rads)');
    xlim([0 max(t(:))]);
    grid on;

    subplot(6,1,3);
    plot(t, DTHETA);
    title([Title, ': Velocity']);
    ylabel('Velocity (rads/s)');
    xlim([0 max(t(:))]);
    grid on;
    
    subplot(6,1,4);
    plot(t, DDTHETA);
    title([Title, ': Acceleration']);
    ylabel('Acceleration (rads/s^2)');
    xlim([0 max(t(:))]);
    grid on;

    subplot(6,1,5);
    plot(t, CURRENT*10^3);
    title([Title, ': Current']);
    ylabel('Current (mA)');
    xlim([0 max(t(:))]);
    grid on;

    subplot(6,1,6);
    plot(t, DCURRENT);
    title([Title, ': Current/s']);
    ylabel('Current (A/s)');
    xlim([0 max(t(:))]);
    grid on;
end
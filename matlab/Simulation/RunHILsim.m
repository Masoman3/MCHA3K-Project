clc;clear;
%% Get Parameters
% Physical World Parameters
g = 9.81;           % Acceleration due to gravity (m/s/s)
alpha = 0*pi/180;   % Ground slope (rad = deg*pi/180)

% Initial Conditions & other sim params
theta0 = 25 * pi/180;    % Initial Theta

tsim = 30;              % Simulation Time (s)
vref_step_point = 500e-3;  % Set point of vref (m/s)
inpdist_step_point = 0.05;

run('Chassis_ss.m')
run('ControllerDesign.m')

%% Simulation
sim('model_hil');

%% Plot Results
h = gcf;
clf(gcf);
hold on;
subplot(4,2,1);
plot(theta.Time, theta.Data*180/pi, 'b-', thetactrl.Time, thetactrl.Data*180/pi, 'g-');
xlabel('Time (s)', 'FontSize', 11);
ylabel('\theta (\circ)', 'FontSize', 14);
legend('Actual', 'Estimated');
ylim([-90 90]);
grid on;

subplot(4,2,3);
plot(dtheta.Time, dtheta.Data*180/pi, 'b-', dthetactrl.Time, dthetactrl.Data*180/pi, 'g--', biasctrl.Time, biasctrl.Data*180/pi, 'r--');
xlabel('Time (s)', 'FontSize', 11);
ylabel('d\theta/dt (\circ)', 'FontSize', 14);
legend('Actual', 'Estimated', 'Bias');
ylim([-500 500]);
grid on;

subplot(4,2,5);
plot(phi.Time, (phi.Data + theta.Data).* r, 'b-', phictrl.Time, (phictrl.Data + thetactrl.Data).* r, 'g--');
xlabel('Time (s)', 'FontSize', 11);
ylabel('x (m)', 'FontSize', 14);
legend('Actual', 'Estimated');
ylim([-10 10]);
grid on;

subplot(4,2,7);
plot(dphi.Time, (dphi.Data + dtheta.Data) .* r, 'b-', dphictrl.Time, (dphictrl.Data + dthetactrl.Data) .* r, 'g--', vref.Time, vref.Data, 'r--');
xlabel('Time (s)', 'FontSize', 11);
ylabel('dx/dt (m/s)', 'FontSize', 14);
legend('Actual', 'Estimated', 'Demanded');
ylim([-2 2]);
grid on;

subplot(4,2,2);
plot(Torque.Time, Torque.Data*10, 'b-');
xlabel('Time (s)', 'FontSize', 11);
ylabel('Control Torque (N.cm)', 'FontSize', 14);
legend('Actual', 'Desired');
ylim([-2 2]);
grid on;

subplot(4,2,6);
plot(time, ML_Current*10^3, 'r-', time, MR_Current*10^3, 'b-');
xlabel('Time (s)', 'FontSize', 11);
ylabel('Current (mA)', 'FontSize', 14);
legend('Left', 'Right');
grid on;

subplot(4,2,4);
plot(ML_Voltage.Time, ML_Voltage.Data, 'r-', MR_Voltage.Time, MR_Voltage.Data, 'b-');
legend('Left', 'Right');
ylabel('Control Voltage (V)', 'FontSize', 14);
xlabel('Time (s)', 'FontSize', 11);
ylim([-15, 15]);
grid on;

h.NextPlot = 'add';
a = axes;
ht = title(['IC: \theta = ' num2str(theta0*180/pi) '\circ'], 'FontSize', 16);
a.Visible = 'off';
ht.Visible = 'on';
hold off;
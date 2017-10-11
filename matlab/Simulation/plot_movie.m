%% This routine makes an animation of the quadrotor simulation.

clc;

disp('Animation started');

%% Declaration of auxiliar vectors

%xref=x(:,1);
%yref=y(:,1);


%% Allocation space movie
mov(1:length(tout)) = struct('cdata',[],'colormap',[]);

%% Dimmension of the figure window for the movie

scrsz = get(0,'ScreenSize');
figmovie=figure('Name','Movie: Quadrotor Trajectory','Position',[scrsz(3)/6 scrsz(4)/6 scrsz(3)*4/6 scrsz(4)*4/6]);

%% We plot the current value and the static ones from the previous declaration.

for k=1:length(tout)
    
figmovie;clf
title('xy-plane Trajectory','FontSize',18)
xlabel('x [m]','FontSize',18)
ylabel('y [m]','FontSize',18)
text(13,0.5,sprintf('Time %0.1f sec',tout(k)),'FontSize',18)
hold on;
plot(xref(1:k),yref(1:k),'r--')
plot(x(1:k,2),y(1:k,2),'b')
plot(xref(k),yref(k),'ro','LineWidth',2);
line([x1(k),x2(k)],[y1(k),y2(k)],'LineWidth',8,'Marker','d','MarkerSize',8)
hold off
xlim([-1 15])
ylim([-1 15])
grid on;

%% Record data
mov(k) = getframe(gcf);
end

disp('Animation finished - Saved in QuadrotorTrajectory.avi')
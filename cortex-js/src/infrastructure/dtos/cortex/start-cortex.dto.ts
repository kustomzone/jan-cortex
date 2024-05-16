import { ApiProperty } from '@nestjs/swagger';
import { IsIP, IsNumber, IsString, Max, Min } from 'class-validator';
import { defaultCortexCppHost, defaultCortexCppPort } from 'constant';

export class StartCortexDto {
  @ApiProperty({
    name: 'host',
    description: 'Cortexcpp host',
    default: defaultCortexCppHost,
  })
  @IsString()
  @IsIP()
  host: string;

  @ApiProperty({
    name: 'port',
    description: 'Cortexcpp port',
    default: defaultCortexCppPort,
  })
  @IsNumber()
  @Min(0)
  @Max(65535)
  port: number;
}